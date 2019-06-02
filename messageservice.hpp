#ifndef MESSAGESERVICE_HPP_
#define MESSAGESERVICE_HPP_

#include <string.h>
#include <vector>
#include <cassert>
#include <queue>
#include <optional>
#include <functional>
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
    using Buffer = std::vector<char>;
    Buffer buffer = Buffer(1024);
    size_t n_bytes_read = 0;

    size_t bufferSize() const { return buffer.size(); }
    size_t chunkSize() const { return bufferSize() - n_bytes_read; }
    char *chunkStart() { return buffer.data() + n_bytes_read; }
    const char *bufferStart() { return buffer.data(); }

    void chunkReceived(size_t n)
    {
      assert(n <= chunkSize());
      n_bytes_read += n;
      assert(n_bytes_read <= bufferSize());
    }

    void resizeBuffer(size_t new_size)
    {
      assert(new_size >= n_bytes_read);
      buffer.resize(new_size);
    }

    void reset(size_t n_bytes_to_keep)
    {
      assert(n_bytes_to_keep <= chunkSize());
      memmove(buffer.data(), chunkStart(), n_bytes_to_keep);
      n_bytes_read = n_bytes_to_keep;
    }
};


class MessageSender {
  public:
    MessageSender() = default;
    MessageSender(const MessageSender &) = delete;
    MessageSender(MessageSender &&) = default;

    void queueMessage(
      const char *message_arg,
      int message_size_arg
    )
    {
      assert(!message_being_sent);
      message_being_sent = message_arg;
      message_size = message_size_arg;
      n_bytes_sent = 0;
    }

    bool messageIsBeingSent() const { return !!message_being_sent; }

    bool
      sendMoreOfTheMessage(
        SocketsInterface &sockets,
        const SocketsInterface::SocketId client_socket_id
      );

  private:
    size_t n_bytes_sent = 0;
    const char *message_being_sent = nullptr;
    size_t message_size = 0;

    size_t nBytesSent() const { return n_bytes_sent; }
    size_t messageSize() const { return message_size; }
    void reset() { message_being_sent = nullptr; }
    const char *chunkStart() const { return message_being_sent + n_bytes_sent; }
    size_t chunkSize() const { return message_size - n_bytes_sent; }

    void chunkSent(size_t n)
    {
      assert(n <= chunkSize());
      n_bytes_sent += n;
      assert(n_bytes_sent <= message_size);
    }
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
    MessageSender message_sender;
    std::queue<std::vector<char>> message_queue;

    void setupNextMessage()
    {
      std::vector<char> &message = message_queue.front();
      message_sender.queueMessage(message.data(), message.size());
    }
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
    struct MessageHandler;

    struct Client {
      Client() = default;
      Client(Client &&) = default;
      std::optional<SocketId> maybe_socket_id;
      MessageReceiver message_receiver;
      QueuedMessageSender queued_message_sender;
    };

    SocketsInterface &sockets;
    std::optional<SocketId> maybe_listen_socket_id;
    std::vector<Client> clients;

    bool isListening() const
    {
      return maybe_listen_socket_id.has_value();
    }

    void acceptConnection(EventInterface &);

    void setupWaitingForConnection(PreSelectParamsInterface &);

    void
      handleWaitingForConnection(
        const PostSelectParamsInterface &,
        EventInterface &
      );

    void closeClientSockets()
    {
      size_t n_clients = clients.size();

      for (size_t i=0; i!=n_clients; ++i) {
        if (clients[i].maybe_socket_id) {
          sockets.close(*clients[i].maybe_socket_id);
          clients[i].maybe_socket_id.reset();
        }
      }
    }

    void closeListenSocket()
    {
      if (maybe_listen_socket_id) {
        sockets.close(*maybe_listen_socket_id);
        maybe_listen_socket_id.reset();
      }
    }

    void closeSockets()
    {
      closeClientSockets();
      closeListenSocket();
    }

    bool isSendingAMessage(const Client &client) const
    {
      return client.queued_message_sender.isSendingAMessage();
    }

    bool isConnected(const Client &client) const
    {
      return client.maybe_socket_id.has_value();
    }

    void setupReceivingMessage(Client &,PreSelectParamsInterface &);
    void setupSendingMessage(Client &,PreSelectParamsInterface &);

    void
      handleReceivingMessage(
        Client &,
        EventInterface &,
        ClientId,
        const PostSelectParamsInterface &
      );

    bool handleSendingMessage(Client &,const PostSelectParamsInterface &);

    void
      disconnectClient(
        ClientId,
        EventInterface &event_handler
      );

    Client &client(ClientId);
};


struct MessageClient {
  public:
    struct EventInterface {
      virtual void connectionRefused() = 0;
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
    struct MessageHandler;

    SocketsInterface &sockets;
    std::optional<SocketsInterface::SocketId> maybe_socket_id;
    bool finished_connecting = false;
    QueuedMessageSender queued_message_sender;
    MessageReceiver message_receiver;

    void setupWaitingForConnection(PreSelectParamsInterface &);
    void
      handleWaitingForConnection(
        EventInterface &,
        const PostSelectParamsInterface &
      );
    void setupReceivingMessage(PreSelectParamsInterface &);
    void setupSendingMessage(PreSelectParamsInterface &);
    void handleSendingMessage(const PostSelectParamsInterface &);

    void
      handleReceivingMessage(
        EventInterface &,
        const PostSelectParamsInterface &
      );
};


#endif /* MESSAGESERVICE_HPP_ */
