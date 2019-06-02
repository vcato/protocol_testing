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
  MessageTestServer(
    SocketsInterface &sockets_arg,
    Terminal &terminal_arg
  );

  bool start();

  bool hasAClient() const { return message_server.nClients() != 0; }

  bool isActive() const
  {
    return message_server.isActive();
  }

  void setupSelect(PreSelectParamsInterface &pre_select)
  {
    message_server.setupSelect(pre_select);
    terminal.setupSelect(pre_select);
  }

  void handleSelect(const PostSelectParamsInterface &post_select)
  {
    message_server.handleSelect(post_select,message_server_event_handler);
    terminal.handleSelect(post_select,terminal_handler);
  }

  EventSinkInterface &eventSink()
  {
    return event_sink;
  }

private:
  using ClientId = MessageServer::ClientId;

  struct TerminalHandler : Terminal::EventInterface {
    MessageServer &server;

    TerminalHandler(MessageServer &server_arg)
    : server(server_arg)
    {
    }

    void gotLine(const std::string &line) override
    {
      for (ClientId client_id : server.clientIds()) {
        server.queueMessageToClient(client_id,line.c_str(),line.length()+1);
      }
    }

    void endOfFile() override
    {
    }
  };

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
  TerminalHandler terminal_handler{message_server};
  ServerEventCallbacks message_server_event_handler{};
  MyEventSink event_sink{*this};
};




class MessageTestClient {
public:
  MessageTestClient(
    SocketsInterface &sockets_arg,
    Terminal &terminal_arg
  );

  void start()
  {
    message_client.startConnecting(messageTestPort());
  }

  void stop()
  {
    message_client.disconnect();
  }

  bool isActive()
  {
    return message_client.isActive();
  }

  void setupSelect(PreSelectParamsInterface &pre_select)
  {
    message_client.setupSelect(pre_select);
    terminal.setupSelect(pre_select);
  }

  EventSinkInterface &eventSink() { return event_sink; }

private:
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

  struct TerminalHandler : Terminal::EventInterface {
    MessageTestClient &message_test_client;

    TerminalHandler(MessageTestClient &message_test_client_arg)
    : message_test_client(message_test_client_arg)
    {
    }

    void gotLine(const std::string &line) override
    {
      message_test_client.gotLineFromTerminal(line);
    }

    void endOfFile() override
    {
      message_test_client.gotEndOfFileFromTerminal();
    }
  };

  SocketsInterface &sockets;
  Terminal &terminal;
  MessageClient message_client{sockets};
  ClientEventCallbacks event_handler;
  TerminalHandler terminal_handler{*this};
  MyEventSink event_sink;

  void handleSelect(const PostSelectParamsInterface &post_select)
  {
    message_client.handleSelect(post_select,event_handler);
    terminal.handleSelect(post_select,terminal_handler);
  }

  void gotLineFromTerminal(const std::string &line);

  void gotEndOfFileFromTerminal()
  {
    message_client.disconnect();
  }
};


#endif /* MESSAGETESTING_HPP_ */
