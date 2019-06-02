#ifndef INTERNETADDRESS_HPP_
#define INTERNETADDRESS_HPP_

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <string>


class InternetAddress {
  public:
    InternetAddress();
    ~InternetAddress();

    void setPort(int port);
    int port() const;
    void setHostname(const std::string &hostname);
    socklen_t sockaddrSize() const;
    sockaddr *sockaddrPtr();
    const sockaddr *sockaddrPtr() const;

  private:
    sockaddr_in address;
};

#endif /* INTERNETADDRESS_HPP_ */
