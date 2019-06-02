#include "messagetesting.hpp"

#include <iostream>
#include <sstream>
#include "messagetestport.hpp"

using std::ostringstream;
using std::string;


MessageTestClient::MessageTestClient(
  SocketsInterface &sockets_arg,
  Terminal &terminal_arg
)
: sockets(sockets_arg),
  terminal(terminal_arg),
  event_sink(*this)
{
  event_handler.connection_refused =
    [this]{
      ostringstream stream;
      stream << "Connection refused.\n";
      terminal.show(stream.str());
    };
  event_handler.got_message =
    [this](const char *message){
      ostringstream stream;
      stream << "Got message: " << message << "\n";
      terminal.show(stream.str());
    };
}


void MessageTestClient::gotLineFromTerminal(const string &line)
{
  ostringstream stream;
  stream << "Sending message " << line << "\n";
  terminal.show(stream.str());
  message_client.queueMessage(line.c_str(),line.length() + 1);
}


MessageTestServer::MessageTestServer(
  SocketsInterface &sockets_arg,
  Terminal &terminal_arg
)
: sockets(sockets_arg),
  terminal(terminal_arg)
{
  message_server_event_handler.got_message =
    [this](ClientId,const char *message)
    {
      ostringstream stream;
      stream << "Got message: " << message << "\n";
      string text = stream.str();
      terminal.show(stream.str());
    };

  message_server_event_handler.client_connected =
    [this](ClientId client_id){
      ostringstream stream;
      stream << "Client " << client_id << " connected.\n";
      terminal.show(stream.str());
    };

  message_server_event_handler.client_disconnected =
    [this](ClientId client_id){
      ostringstream stream;
      stream << "Client " << client_id << " disconnected.\n";
      terminal.show(stream.str());
    };
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
