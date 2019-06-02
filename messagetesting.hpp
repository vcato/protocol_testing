#ifndef MESSAGETESTING_HPP_
#define MESSAGETESTING_HPP_

#include "socketsinterface.hpp"
#include "terminal.hpp"
#include "processevents.hpp"
#include "messageservice.hpp"


inline int messageTestPort() { return 4145; }


class MessageTestServer {
public:
  MessageTestServer(SocketsInterface &,Terminal &);

  bool start();
  bool hasAClient() const;
  bool isActive() const;
  EventSinkInterface &eventSink() { return event_sink; }

private:
  using ClientId = MessageServer::ClientId;

  struct TerminalHandler;
  struct ServerHandler;

  struct MyEventSink : EventSinkInterface {
    MessageTestServer &message_test_server;

    MyEventSink(MessageTestServer &message_test_server_arg)
    : message_test_server(message_test_server_arg)
    {
    }

    void setupSelect(PreSelectParamsInterface &pre_select) const override
    {
      message_test_server.setupSelect(pre_select);
    }

    void handleSelect(const PostSelectParamsInterface &post_select) override
    {
      message_test_server.handleSelect(post_select);
    }
  };

  SocketsInterface &sockets;
  Terminal &terminal;
  MessageServer message_server{sockets};
  MyEventSink event_sink{*this};

  void gotMessageFromClient(ClientId,const char *message);
  void clientConnected(ClientId client_id);
  void clientDisconnected(ClientId client_id);
  void gotLineFromTerminal(const std::string &);
  void setupSelect(PreSelectParamsInterface &pre_select);
  void handleSelect(const PostSelectParamsInterface &post_select);
  void sendMessageToClient(ClientId,const std::string &);
};


class MessageTestClient {
public:
  MessageTestClient(SocketsInterface &,Terminal &);

  void start() { message_client.startConnecting(messageTestPort()); }
  void stop() { message_client.disconnect(); }
  bool isActive() { return message_client.isActive(); }
  EventSinkInterface &eventSink() { return event_sink; }

private:
  struct TerminalHandler;
  struct ClientHandler;

  struct MyEventSink : EventSinkInterface {
    MessageTestClient &message_test_client;

    MyEventSink(MessageTestClient &message_test_client_arg)
    : message_test_client(message_test_client_arg)
    {
    }

    void setupSelect(PreSelectParamsInterface &pre_select) const override
    {
      message_test_client.setupSelect(pre_select);
    }

    void handleSelect(const PostSelectParamsInterface &post_select) override
    {
      message_test_client.handleSelect(post_select);
    }
  };

  SocketsInterface &sockets;
  Terminal &terminal;
  MessageClient message_client{sockets};
  MyEventSink event_sink;

  void setupSelect(PreSelectParamsInterface &pre_select);
  void handleSelect(const PostSelectParamsInterface &post_select);
  void gotLineFromTerminal(const std::string &line);
  void gotEndOfFileFromTerminal() { message_client.disconnect(); }
  void connectionRefused();
  void connected();
  void gotMessageFromServer(const char *message);
  void sendMessage(const std::string &message);
};


#endif /* MESSAGETESTING_HPP_ */
