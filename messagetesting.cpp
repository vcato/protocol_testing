#include "messagetesting.hpp"

#include <iostream>
#include <sstream>
#include "messagetestport.hpp"

using std::ostringstream;
using std::string;


struct MessageTestClient::TerminalHandler : Terminal::EventInterface {
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


void MessageTestClient::setupSelect(PreSelectParamsInterface &pre_select)
{
  message_client.setupSelect(pre_select);
  terminal.setupSelect(pre_select);
}


void MessageTestClient::connectionRefused()
{
  ostringstream stream;
  stream << "Connection refused.\n";
  terminal.show(stream.str());
}


void MessageTestClient::gotMessageFromServer(const char *message)
{
  ostringstream stream;
  stream << "Got message: " << message << "\n";
  terminal.show(stream.str());
}


struct MessageTestClient::ClientHandler : MessageClient::EventInterface {
  MessageTestClient &message_test_client;

  ClientHandler(MessageTestClient &message_test_client_arg)
  : message_test_client(message_test_client_arg)
  {
  }

  virtual void connectionRefused()
  {
    message_test_client.connectionRefused();
  }

  virtual void gotMessage(const char *message)
  {
    message_test_client.gotMessageFromServer(message);
  }
};


MessageTestClient::MessageTestClient(
  SocketsInterface &sockets_arg,
  Terminal &terminal_arg
)
: sockets(sockets_arg),
  terminal(terminal_arg),
  event_sink(*this)
{
}


void MessageTestClient::gotLineFromTerminal(const string &line)
{
  ostringstream stream;
  stream << "Sending message " << line << "\n";
  terminal.show(stream.str());
  message_client.queueMessage(line.c_str(),line.length() + 1);
}


void
  MessageTestClient::handleSelect(const PostSelectParamsInterface &post_select)
{
  ClientHandler event_handler{*this};
  message_client.handleSelect(post_select,event_handler);
  TerminalHandler terminal_handler{*this};
  terminal.handleSelect(post_select,terminal_handler);
}


struct MessageTestServer::ServerHandler : MessageServer::EventInterface {
  MessageTestServer &message_test_server;

  ServerHandler(MessageTestServer &message_test_server_arg)
  : message_test_server(message_test_server_arg)
  {
  }

  virtual void gotMessage(ClientId client_id,const char *message)
  {
    message_test_server.gotMessageFromClient(client_id,message);
  }

  virtual void clientConnected(ClientId client_id)
  {
    message_test_server.clientConnected(client_id);
  }

  virtual void clientDisconnected(ClientId client_id)
  {
    message_test_server.clientDisconnected(client_id);
  }
};


struct MessageTestServer::TerminalHandler : Terminal::EventInterface {
  MessageTestServer &message_test_server;

  TerminalHandler(
    MessageTestServer &message_test_server_arg
  )
  : message_test_server(message_test_server_arg)
  {
  }

  void gotLine(const std::string &line) override
  {
    message_test_server.gotLineFromTerminal(line);
  }

  void endOfFile() override
  {
  }
};


void MessageTestServer::setupSelect(PreSelectParamsInterface &pre_select)
{
  message_server.setupSelect(pre_select);
  terminal.setupSelect(pre_select);
}


void MessageTestServer::gotMessageFromClient(ClientId,const char *message)
{
  ostringstream stream;
  stream << "Got message: " << message << "\n";
  string text = stream.str();
  terminal.show(stream.str());
}


void MessageTestServer::clientConnected(ClientId client_id)
{
  ostringstream stream;
  stream << "Client " << client_id << " connected.\n";
  terminal.show(stream.str());
}


void MessageTestServer::clientDisconnected(ClientId client_id)
{
  ostringstream stream;
  stream << "Client " << client_id << " disconnected.\n";
  terminal.show(stream.str());
}


void MessageTestServer::gotLineFromTerminal(const string &line)
{
  for (ClientId client_id : message_server.clientIds()) {
    message_server.queueMessageToClient(client_id,line.c_str(),line.length()+1);
  }
}


MessageTestServer::MessageTestServer(
  SocketsInterface &sockets_arg,
  Terminal &terminal_arg
)
: sockets(sockets_arg),
  terminal(terminal_arg)
{
}


bool MessageTestServer::start()
{
  try {
    message_server.startListening(messageTestPort());
  }
  catch (std::runtime_error &error) {
    ostringstream stream;
    stream << "Unable to start listening: " << error.what() << "\n";
    terminal.show(stream.str());
    return false;
  }

  ostringstream stream;
  stream << "Waiting for connection on port " << messageTestPort() << "\n";
  terminal.show(stream.str());

  return true;
}


void
  MessageTestServer::handleSelect(
    const PostSelectParamsInterface &post_select
  )
{
  ServerHandler server_handler{*this};
  message_server.handleSelect(post_select,server_handler);
  TerminalHandler terminal_handler{*this};
  terminal.handleSelect(post_select,terminal_handler);
}
