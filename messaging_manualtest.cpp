#include <iostream>
#include "messageservice.hpp"
#include "systemsockets.hpp"
#include "systemselector.hpp"
#include "systemterminal.hpp"
#include "processevents.hpp"
#include "messagetesting.hpp"

using std::cerr;
using std::string;
using std::vector;


static int runServer()
{
  SystemSockets sockets;
  SystemSelector selector;
  SystemTerminal terminal;
  MessageTestServer server{sockets,terminal};
  vector<EventSinkInterface*> event_sinks{&server.eventSink()};

  server.start();

  while (server.isActive()) {
    processEvents(selector,event_sinks);
  }

  return EXIT_SUCCESS;
}



static int runClient()
{
  SystemSockets sockets;
  SystemTerminal terminal;
  SystemSelector selector;
  MessageTestClient client{sockets,terminal};
  vector<EventSinkInterface *> event_sinks = {&client.eventSink()};

  client.start();

  while (client.isActive()) {
    processEvents(selector,event_sinks);
  }

  return EXIT_SUCCESS;
}



static int handleOperation(const string &operation)
{
  if (operation == "server") {
    return runServer();
  }

  if (operation == "client") {
    return runClient();
  }

  cerr << "Unknown operation: " << operation << "\n";
  return EXIT_FAILURE;
}


int main(int argc,char** argv)
{
  if (argc == 2) {
    string operation = argv[1];
    return handleOperation(operation);
  }

  cerr << "Usage: main <server|client>\n";
  return EXIT_FAILURE;
}
