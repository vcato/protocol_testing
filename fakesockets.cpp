#include "fakesockets.hpp"

using SocketId = FakeSockets::SocketId;
using std::optional;


FakeSockets::FakeSockets(
  FakeFileDescriptorAllocator &file_descriptor_allocator_arg
)
: file_descriptor_allocator(file_descriptor_allocator_arg)
{
}


SocketId FakeSockets::allocate()
{
  size_t fd = file_descriptor_allocator.allocate();

  if (fd >= sockets.size()) {
    sockets.resize(fd + 1);
  }

  assert(!sockets[fd]);
  sockets[fd].emplace();
  return fd;
}


void FakeSockets::deallocate(SocketId socket_id)
{
  assert(sockets[socket_id]);
  sockets[socket_id].reset();
  file_descriptor_allocator.deallocate(socket_id);
}


void FakeSockets::setNonBlocking(SocketId socket_id,bool non_blocking)
{
  socket(socket_id).is_non_blocking = non_blocking;
}


void
  FakeSockets::connect(
    SocketId socket_id,
    const InternetAddress &address
  )
{
  Socket &socket = this->socket(socket_id);
  socket.connect(address.port());
}


bool FakeSockets::connectionWasRefused(SocketId socket_id)
{
  return socket(socket_id).connection_was_refused;
}


bool FakeSockets::anySocketsIsBoundToPort(int port) const
{
  for (const optional<Socket> &maybe_socket : sockets) {
    if (maybe_socket) {
      if (maybe_socket->maybe_bound_port == port) {
        return true;
      }
    }
  }

  return false;
}


void FakeSockets::bind(SocketId sockfd,const InternetAddress &address)
{
  if (anySocketsIsBoundToPort(address.port())) {
    throw std::runtime_error("Unable to bind socket.");
  }

  socket(sockfd).bind(address.port());
}


void FakeSockets::listen(SocketId sockfd, int /*backlog*/)
{
  socket(sockfd).listen();
}


int FakeSockets::accept(SocketId socket_id)
{
  optional<SocketId> maybe_client_socket_id =
    findSocketConnectedToSocket(socket_id);

  assert(maybe_client_socket_id);

  SocketId client_socket_id = *maybe_client_socket_id;
  SocketId new_socket_id = allocate();

  socket(client_socket_id).maybe_remote_socket_id = new_socket_id;
  socket(new_socket_id).maybe_remote_socket_id = client_socket_id;
  return new_socket_id;
}


optional<SocketId> FakeSockets::findSocketIdListeningOnPort(int port)
{
  assert(port != 0);

  for (int i=0; i!=nSocketIds(); ++i) {
    if (Socket *socket_ptr = maybeSocket(i)) {
      if (socket_ptr->isListeningOnPort(port)) {
        return i;
      }
    }
  }

  return std::nullopt;
}


bool FakeSockets::checkWrite(SocketId socket_id)
{
  Socket &socket = this->socket(socket_id);

  if (socket.isConnecting()) {
    // See if there is another socket that is listening on the
    // same port we are connecting to.
    assert(socket.maybe_connect_port);
    optional<SocketId> maybe_listen_socket_id =
      findSocketIdListeningOnPort(*socket.maybe_connect_port);

    if (maybe_listen_socket_id) {
      socket.maybe_connect_port.reset();
      socket.maybe_remote_socket_id = *maybe_listen_socket_id;
    }
    else {
      socket.connection_was_refused = true;
      socket.maybe_connect_port.reset();
    }

    return true;
  }

  if (socket.maybe_remote_socket_id) {
    SocketId remote_socket_id = *socket.maybe_remote_socket_id;

    if (!socket.output_buffer.isFull()) {
      return true;
    }

    if (this->socket(remote_socket_id).is_closed) {
      // We'll get an error on write, but we consider this to be writable
      // since we won't block.
      return true;
    }

    return false;
  }

  if (socket.connection_was_refused) {
    return true;
  }

  assert(false);
}


bool FakeSockets::checkExcept(SocketId socket_id)
{
  Socket &socket = this->socket(socket_id);

  if (socket.isConnecting()) {
    assert(false);
  }
  else {
    return false;
  }
}


optional<SocketId> FakeSockets::findSocketConnectedToSocket(SocketId socket_id)
{
  int n_sockets = nSocketIds();

  for (int i=0; i!=n_sockets; ++i) {
    if (Socket *socket_ptr = maybeSocket(i)) {
      if (socket_ptr->maybe_remote_socket_id == socket_id) {
        return i;
      }
    }
  }

  return std::nullopt;
}


int FakeSockets::nAllocated() const
{
  int n = sockets.size();
  int count = 0;

  for (int i=0; i!=n; ++i) {
    if (sockets[i]) {
      ++count;
    }
  }

  return count;
}


bool FakeSockets::checkRead(SocketId socket_id)
{
  Socket &socket = this->socket(socket_id);

  if (socket.isConnecting()) {
    assert(false);
  }
  else if (socket.is_listening) {
    // Look for sockets which are connected to this socket, and make
    // them be connected to a new socket instead.
    optional<SocketId> maybe_client_socket_id =
      findSocketConnectedToSocket(socket_id);

    if (maybe_client_socket_id) {
      // We're listening, and a socket made a connection.
      return true;
    }
    else {
      return false;
    }
  }
  else if (socket.maybe_remote_socket_id) {
    SocketId remote_socket_id = *socket.maybe_remote_socket_id;
    Socket &remote_socket = this->socket(remote_socket_id);

    if (remote_socket.is_closed) {
      return true;
    }

    if (!remote_socket.output_buffer.isEmpty()) {
      return true;
    }

    return false;
  }
  else {
    assert(false);
  }
}


void FakeSockets::select(FakeSelectParams &select_params)
{
  int n_still_set = 0;
  int n = nSocketIds();

  for (int i=0; i!=n; ++i) {
    if (select_params.read_set[i]) {
      if (maybeSocket(i)) {
        if (checkRead(i)) {
          ++n_still_set;
        }
        else {
          select_params.read_set[i] = false;
        }
      }
    }
  }

  for (int i=0; i!=n; ++i) {
    if (select_params.write_set[i]) {
      if (maybeSocket(i)) {
        if (checkWrite(i)) {
          ++n_still_set;
        }
        else {
          select_params.write_set[i] = false;
        }
      }
    }
  }


  for (int i=0; i!=n; ++i) {
    if (select_params.except_set[i]) {
      if (checkExcept(i)) {
        assert(false);
      }
      else {
        select_params.except_set[i] = false;
      }
    }
  }
}


int FakeSockets::send(SocketId sockfd, const void * buf, size_t len)
{
  Socket &socket = this->socket(sockfd);

  if (socket.maybe_remote_socket_id) {
    SocketId remote_socket_id = *socket.maybe_remote_socket_id;

    if (this->socket(remote_socket_id).is_closed) {
      return 0; // report EOF
    }
  }

  if (socket.maybe_n_bytes_before_send_error) {
    size_t n_bytes_before_send_error = *socket.maybe_n_bytes_before_send_error;

    if (n_bytes_before_send_error == 0) {
      return -1;
    }

    if (n_bytes_before_send_error < len) {
      size_t n_bytes_to_send = n_bytes_before_send_error;
      *socket.maybe_n_bytes_before_send_error -= n_bytes_to_send;
      assert(*socket.maybe_n_bytes_before_send_error == 0);
      return socket.output_buffer.put(buf,n_bytes_to_send);
    }
    else {
      assert(false);
    }

    *socket.maybe_n_bytes_before_send_error -= len;
  }

  return socket.output_buffer.put(buf,len);
}


int FakeSockets::recv(SocketId sockfd, void *buf, size_t len)
{
  Socket &socket = this->socket(sockfd);

  if (socket.maybe_n_bytes_before_recv_error) {
    size_t &n_bytes_before_recv_error = *socket.maybe_n_bytes_before_recv_error;

    if (n_bytes_before_recv_error == 0) {
      return -1;
    }

    if (n_bytes_before_recv_error < len) {
      len = n_bytes_before_recv_error;
    }

    n_bytes_before_recv_error -= len;
  }

  assert(socket.maybe_remote_socket_id);
  SocketId remote_socket_id = *socket.maybe_remote_socket_id;
  Socket &remote_socket = this->socket(remote_socket_id);
  return remote_socket.output_buffer.get(buf,len);
}


void FakeSockets::close(SocketId sockfd)
{
  Socket &socket = this->socket(sockfd);
  assert(!socket.is_closed);

  if (socket.maybe_remote_socket_id) {
    SocketId remote_socket_id = *socket.maybe_remote_socket_id;
    Socket &remote_socket = this->socket(remote_socket_id);

    if (remote_socket.is_closed) {
      deallocate(sockfd);
      deallocate(remote_socket_id);
    }
    else {
      socket.is_closed = true;
    }
  }
  else if (socket.is_listening) {
    deallocate(sockfd);
  }
  else {
    // Socket was created but never started listening or connected.
    // There may have been an error trying to connect.
    deallocate(sockfd);
  }
}


void FakeSockets::setNBytesBeforeRecvError(SocketId socket_id, size_t n_bytes)
{
  socket(socket_id).maybe_n_bytes_before_recv_error = n_bytes;
}


void FakeSockets::setNBytesBeforeSendError(SocketId socket_id, size_t n_bytes)
{
  socket(socket_id).maybe_n_bytes_before_send_error = n_bytes;
}
