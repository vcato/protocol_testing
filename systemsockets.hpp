#include "socketsinterface.hpp"


class SystemSockets : public SocketsInterface {
  public:
    int create() override;
    void setNonBlocking(SocketId socket_id,bool non_blocking) override;
    void bind(SocketId sockfd,const InternetAddress &) override;
    void listen(SocketId sockfd, int backlog) override;
    void connect(SocketId sockfd, const InternetAddress &) override;
    bool connectionWasRefused(SocketId) override;
    int accept(SocketId sockfd) override;
    int send(SocketId sockfd, const void *buf, size_t len) override;
    int recv(SocketId sockfd, void *buf, size_t len) override;
    void close(SocketId sockfd) override;
};
