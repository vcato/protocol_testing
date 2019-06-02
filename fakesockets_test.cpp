#include "fakesockets.hpp"

#include "fakeselector.hpp"


using SocketId = FakeSockets::SocketId;


namespace {
struct Connection {
  const SocketId server_socket_id;
  const SocketId client_socket_id;
};
}


static int testPort()
{
  return 1234;
}


static SocketId listenOn(FakeSockets &sockets)
{
  SocketId listen_socket_id = sockets.create();
  InternetAddress address;
  address.setPort(testPort());
  sockets.bind(listen_socket_id,address);
  sockets.listen(listen_socket_id,/*backlog*/1);
  return listen_socket_id;
}


static bool canWrite(FakeSockets &sockets,SocketId socket_id)
{
  FakeSelector selector({&sockets});
  selector.beginSelect();
  selector.preSelectParams().setWrite(socket_id);
  selector.callSelect();
  bool can_write = selector.postSelectParams().writeIsSet(socket_id);
  selector.endSelect();
  return can_write;
}


static SocketId connect(FakeSockets &sockets)
{
  SocketId socket_id = sockets.create();
  sockets.setNonBlocking(socket_id,true);
  InternetAddress address;
  address.setPort(testPort());
  sockets.connect(socket_id,address);
  assert(canWrite(sockets,socket_id));
  sockets.setNonBlocking(socket_id,false);
  return socket_id;
}


static Connection createConnection(FakeSockets &sockets)
{
  SocketId listen_socket_id = listenOn(sockets);
  SocketId client_socket_id = connect(sockets);
  SocketId server_socket_id = sockets.accept(listen_socket_id);
  return Connection{server_socket_id,client_socket_id};
}


static void testSetNBytesBeforeRecvError()
{
  FakeFileDescriptorAllocator file_descriptor_allocator;
  FakeSockets sockets(file_descriptor_allocator);

  Connection connection = createConnection(sockets);
  SocketId server_socket_id = connection.server_socket_id;
  SocketId client_socket_id = connection.client_socket_id;

  {
    const char buffer[] = {'1','2'};
    int send_result = sockets.send(server_socket_id, buffer, sizeof buffer);
    assert(send_result == 2);
  }

  sockets.setNBytesBeforeRecvError(client_socket_id,1);

  {
    char buffer[2] = {};
    int recv_result = sockets.recv(client_socket_id, buffer, sizeof buffer);
    assert(recv_result == 1);
    assert(buffer[0] == '1');
  }
  {
    char buffer[2] = {};
    int recv_result = sockets.recv(client_socket_id, buffer, sizeof buffer);
    assert(recv_result == -1);
  }
}


int main()
{
  testSetNBytesBeforeRecvError();
}
