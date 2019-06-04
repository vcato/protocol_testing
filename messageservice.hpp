#ifndef MESSAGESERVICE_HPP_
#define MESSAGESERVICE_HPP_

#include <vector>
#include <queue>
#include <optional>
#include "socketsinterface.hpp"
#include "selectparams.hpp"


class MessageReceiver {
  public:
    struct EventInterface {
      virtual void gotMessage(const char *message) = 0;
    };

    bool
      receiveMoreOfTheMessage(
        SocketsInterface &,
        EventInterface &,
        SocketsInterface::SocketId
      );

  private:
    struct Impl;
    using Buffer = std::vector<char>;

    Buffer buffer = Buffer(1024);
    size_t n_bytes_read = 0;
};


class MessageSender {
  public:
    MessageSender() = default;
    MessageSender(const MessageSender &) = delete;
    MessageSender(MessageSender &&) = default;

    void queueMessage(const char *message_arg, int message_size_arg);
    bool messageIsBeingSent() const { return !!message_being_sent; }

    bool
      sendMoreOfTheMessage(
        SocketsInterface &sockets,
        const SocketsInterface::SocketId client_socket_id
      );

  private:
    struct Impl;

    size_t n_bytes_sent = 0;
    const char *message_being_sent = nullptr;
    size_t message_size = 0;
};



class QueuedMessageSender {
  public:
    bool isSendingAMessage() const { return !message_queue.empty(); }

    bool
      handleSendingMessage(
        SocketsInterface &sockets,
        SocketsInterface::SocketId socket_id,
        const PostSelectParamsInterface &post_select_params
      );

    void queueMessage(const char *message,int message_size);

  private:
    struct Impl;

    MessageSender message_sender;
    std::queue<std::vector<char>> message_queue;
};


class MessageServer {
  public:
    using ClientId = int;
    using SocketId = SocketsInterface::SocketId;

    struct EventInterface {
      using ClientId = MessageServer::ClientId;
      virtual void gotMessage(ClientId,const char *) = 0;
      virtual void clientConnected(ClientId) = 0;
      virtual void clientDisconnected(ClientId) = 0;
    };

    MessageServer(SocketsInterface &sockets_arg);
    MessageServer(const MessageServer &) = delete;
    MessageServer(MessageServer &&) = delete;
    ~MessageServer();

    void startListening(int port);
    void stopListening();
    bool isActive() const;
    void setupSelect(PreSelectParamsInterface &);
    void handleSelect(const PostSelectParamsInterface &,EventInterface &);
    std::vector<ClientId> clientIds() const;
    int nClients() const { return clientIds().size(); }
    bool isSendingAMessageTo(ClientId client_id) const;
    SocketId clientSocketId(ClientId) const;

    void
      queueMessageToClient(
        ClientId,
        const char *message_arg,
        int message_size_arg
      );

  private:
    struct Impl;
    struct Client;

    SocketsInterface &sockets;
    std::optional<SocketId> maybe_listen_socket_id;
    std::vector<Client> clients;
};


struct MessageClient {
  public:
    struct EventInterface {
      virtual void connectionRefused() = 0;
      virtual void connected() = 0;
      virtual void gotMessage(const char *) = 0;
    };

    MessageClient(SocketsInterface &sockets_arg);
    MessageClient(const MessageClient &) = delete;
    MessageClient(MessageClient &&) = delete;

    void startConnecting(int port);
    bool isActive() const;
    bool isConnected() const;
    void setupSelect(PreSelectParamsInterface &);
    void handleSelect(const PostSelectParamsInterface &,EventInterface &);
    bool isSendingAMessage() const;
    void queueMessage(const char *message_arg,int message_size_arg);
    void disconnect();

  private:
    struct Impl;

    SocketsInterface &sockets;
    std::optional<SocketsInterface::SocketId> maybe_socket_id;
    bool finished_connecting = false;
    QueuedMessageSender queued_message_sender;
    MessageReceiver message_receiver;
};


#endif /* MESSAGESERVICE_HPP_ */
