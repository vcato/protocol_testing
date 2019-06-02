#include "systemsockets.hpp"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <iostream>
#include <cassert>

using std::cerr;
using SocketId = SystemSockets::SocketId;


void SystemSockets::bind(SocketId sockfd,const InternetAddress &address)
{
  const sockaddr *addr = address.sockaddrPtr();
  socklen_t addrlen = address.sockaddrSize();

  int bind_result = ::bind(sockfd,addr,addrlen);

  if (bind_result == -1) {
    throw std::runtime_error("Unable to bind socket.");
  }
}


int SystemSockets::send(SocketId sockfd, const void *buf, size_t len)
{
  int send_result = ::send(sockfd,buf,len,/*flags*/0);

  if (send_result < 0) {
    cerr << strerror(errno) << "\n";
    assert(false);
  }

  return send_result;
}


int SystemSockets::recv(SocketId sockfd, void *buf, size_t len)
{
  return ::recv(sockfd,buf,len,/*flags*/0);
}


void SystemSockets::close(SocketId sockfd)
{
  int close_result = ::close(sockfd);

  if (close_result == -1) {
    throw std::runtime_error("Error closing socket.");
  }
}


SocketId SystemSockets::create()
{
  int domain = AF_INET;
  int type = SOCK_STREAM;
  int protocol = 0;
  int socket_result = socket(domain,type,protocol);

  if (socket_result == -1) {
    throw std::runtime_error("Failed to create socket.");
  }

  return socket_result;
}


void SystemSockets::setNonBlocking(SocketId socket_id,bool non_blocking)
{
#ifdef _WIN32
  unsigned long value = non_blocking ? 1 : 0;
  int result = ioctlsocket(socket_handle, FIONBIO, &value);
#else
  int flags = fcntl(socket_id, F_GETFL, 0);

  if (flags == -1) {
    throw std::runtime_error("Unable to set non-blocking");
  }

  if (non_blocking) {
    flags |= O_NONBLOCK;
  }
  else {
    flags &= ~O_NONBLOCK;
  }

  int result = fcntl(socket_id, F_SETFL, flags);
#endif

  if (result!=0) {
    throw std::runtime_error("Unable to set non-blocking");
  }
}


void SystemSockets::listen(SocketId socket_id, int backlog)
{
  int listen_result = ::listen(socket_id,backlog);

  if (listen_result == -1) {
    throw std::runtime_error("Unable to listen.");
  }
}


void
  SystemSockets::connect(
    SocketId sockfd,
    const InternetAddress &server_address
  )
{
  const sockaddr *addr = server_address.sockaddrPtr();
  socklen_t addrlen = server_address.sockaddrSize();
  int connect_result = ::connect(sockfd,addr,addrlen);

  if (connect_result == -1) {
    if (errno == EINPROGRESS) {
      return;
    }

    cerr << "errno: " << errno << "\n";
    throw std::runtime_error("Unable to connect to server.");
  }
}


bool SystemSockets::connectionWasRefused(SocketId socket_id)
{
  int error = 0;
  socklen_t size = sizeof error;
  int getsockopt_result =
    getsockopt(socket_id, SOL_SOCKET, SO_ERROR, &error, &size);

  if (getsockopt_result == -1) {
    assert(false);
  }

  if (error == 0) {
    return false;
  }

  if (error == ECONNREFUSED) {
    return true;
  }

  cerr << "SystemSockets::connectionWasRefused: error: " <<
    strerror(error) << "\n";
  assert(false);
}


auto SystemSockets::accept(SocketId sockfd) -> SocketId
{
  InternetAddress client_address;
  sockaddr *addr = client_address.sockaddrPtr();
  socklen_t addrlen = client_address.sockaddrSize();
  int accept_result = ::accept(sockfd,addr,&addrlen);

  if (accept_result == -1) {
    // Error accepting
    assert(false);
  }

  return accept_result;
}
