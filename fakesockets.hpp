#ifndef FAKESOCKETS_HPP_
#define FAKESOCKETS_HPP_


#include <vector>
#include <optional>
#include "socketsinterface.hpp"
#include "buffer.hpp"
#include "fakefiledescriptorallocator.hpp"
#include "fakeselectable.hpp"


class FakeSockets : public SocketsInterface, public FakeSelectable {
  public:
    FakeSockets(FakeFileDescriptorAllocator &file_descriptor_allocator_arg)
    : file_descriptor_allocator(file_descriptor_allocator_arg)
    {
    }
    int create() override { return allocate(); }
    void setNonBlocking(SocketId socket_id,bool non_blocking) override;
    void connect(SocketId socket_id, const InternetAddress &address) override;
    void bind(SocketId sockfd,const InternetAddress &address) override;
    void listen(SocketId sockfd, int /*backlog*/) override;
    int accept(SocketId socket_id) override;
    int send(SocketId sockfd, const void * buf, size_t len) override;
    int recv(SocketId sockfd, void *buf, size_t len) override;
    void close(SocketId sockfd) override;

    int nSocketIds() const { return sockets.size(); }
    int nFileDescriptors() const { return nSocketIds(); }
    int nAllocated() const;
    void select(FakeSelectParams &);
    void setNBytesBeforeRecvError(SocketId, size_t n);
    void setNBytesBeforeSendError(SocketId, size_t n);

  private:
    struct Socket {
      std::optional<int> maybe_bound_port;
      bool connection_was_refused = false;
      bool is_listening = false;
      bool is_non_blocking = false;
      bool is_closed = false;
      std::optional<int> maybe_connect_port;
      std::optional<SocketId> maybe_remote_socket_id;
      std::optional<size_t> maybe_n_bytes_before_recv_error;
      std::optional<size_t> maybe_n_bytes_before_send_error;
      Buffer output_buffer{/*capacity*/2};

      bool isBound() const { return maybe_bound_port.has_value(); }
      void bind(int port) { maybe_bound_port = port; }

      bool isListeningOnPort(int port) const
      {
        return is_listening && maybe_bound_port == port;
      }

      void listen()
      {
        assert(isBound());
        assert(!is_listening);
        is_listening = true;
      }

      void connect(int port)
      {
        assert(is_non_blocking);
        assert(!maybe_connect_port);
        maybe_connect_port = port;
      }

      bool isConnecting() const
      {
        return maybe_connect_port.has_value();
      }
    };

    FakeFileDescriptorAllocator &file_descriptor_allocator;
    std::vector<std::optional<Socket>> sockets;

    SocketId allocate();
    void deallocate(SocketId socket_id);

    Socket *maybeSocket(int i)
    {
      if (sockets[i]) {
        return &*sockets[i];
      }
      else {
        return nullptr;
      }
    }

    Socket &socket(int i)
    {
      Socket *socket_ptr = maybeSocket(i);
      assert(socket_ptr);
      return *socket_ptr;
    }

    bool anySocketsIsBoundToPort(int port) const;

    bool connectionWasRefused(SocketId socket_id);
    std::optional<SocketId> findSocketConnectedToSocket(SocketId socket_id);
    std::optional<SocketId> findSocketIdListeningOnPort(int port);
    bool checkRead(SocketId socket_id);
    bool checkWrite(SocketId socket_id);
    bool checkExcept(SocketId socket_id);
};


#endif /* FAKESOCKETS_HPP_ */
