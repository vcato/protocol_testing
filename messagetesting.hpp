#ifndef MESSAGETESTING_HPP_
#define MESSAGETESTING_HPP_


#include <iostream>
#include "socketsinterface.hpp"
#include "terminal.hpp"
#include "processevents.hpp"
#include "messageservice.hpp"
#include "messagetestport.hpp"


class MessageTestServer {
public:
  MessageTestServer(SocketsInterface &,Terminal &);

  bool start();
  bool hasAClient() const { return message_server.nClients() != 0; }
  bool isActive() const { return message_server.isActive(); }
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
  void gotMessageFromServer(const char *message);
};


#endif /* MESSAGETESTING_HPP_ */
