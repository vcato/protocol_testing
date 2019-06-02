#ifndef SOCKETSINTERFACE_HPP_
#define SOCKETSINTERFACE_HPP_


#include "internetaddress.hpp"


struct SocketsInterface {
  using SocketId = int;

  virtual SocketId create() = 0;
  virtual void setNonBlocking(SocketId,bool non_blocking) = 0;
  virtual void connect(SocketId, const InternetAddress &) = 0;
  virtual bool connectionWasRefused(SocketId) = 0;
  virtual void bind(SocketId,const InternetAddress &) = 0;
  virtual void listen(SocketId, int backlog) = 0;
  virtual SocketId accept(SocketId) = 0;
  virtual int recv(SocketId, void *buf, size_t len) = 0;
  virtual int send(SocketId, const void *buf, size_t len) = 0;
  virtual void close(SocketId) = 0;
};


#endif /* SOCKETSINTERFACE_HPP_ */
