#include "messagetesting.hpp"

#include "fakesockets.hpp"
#include "faketerminal.hpp"
#include "fakeselector.hpp"

using std::cerr;
using std::string;


namespace {
struct Tester {
  FakeFileDescriptorAllocator file_descriptor_allocator;
  FakeSockets sockets{file_descriptor_allocator};
  FakeSelector selector{{&sockets}};
  std::vector<EventSinkInterface*> event_sinks;

  void addEventSink(EventSinkInterface &event_sink)
  {
    event_sinks.push_back(&event_sink);
  }

  void waitForOutput(FakeTerminal &terminal,const string &expected_output)
  {
    while (terminal.output() != expected_output) {
      processEvents(selector,event_sinks);
    }

    terminal.clearOutput();
  }
};
}


namespace {
struct ClientServerTester : Tester {
  FakeTerminal server_terminal{file_descriptor_allocator};
  FakeTerminal client_terminal{file_descriptor_allocator};
  MessageTestServer server{sockets,server_terminal};
  MessageTestClient client{sockets,client_terminal};

  ClientServerTester()
  {
    addEventSink(server.eventSink());
    addEventSink(client.eventSink());
    selector.addSelectable(server_terminal);
    selector.addSelectable(client_terminal);
  }

  void waitForServerTerminalOutput(const string &expected_output)
  {
    waitForOutput(server_terminal,expected_output);
  }

  void waitForClientTerminalOutput(const string &expected_output)
  {
    waitForOutput(client_terminal,expected_output);
  }

  void addClientTerminalInput(const string &input)
  {
    client_terminal.addInput(input);
  }

  void addServerTerminalInput(const string &input)
  {
    server_terminal.addInput(input);
  }
};
}


namespace {
struct TwoServerTester : Tester {
  FakeTerminal server1_terminal{file_descriptor_allocator};
  FakeTerminal server2_terminal{file_descriptor_allocator};
  MessageTestServer server1{sockets,server1_terminal};
  MessageTestServer server2{sockets,server2_terminal};

  TwoServerTester()
  {
    addEventSink(server1.eventSink());
    addEventSink(server2.eventSink());
    selector.addSelectable(server1_terminal);
    selector.addSelectable(server2_terminal);
  }

  void waitForServer2TerminalOutput(const string &expected_output)
  {
    waitForOutput(server2_terminal,expected_output);
  }
};
}



static void testNormalUsage()
{
  ClientServerTester tester;

  tester.server.start();
  tester.waitForServerTerminalOutput("Waiting for connection on port 4145\n");
  tester.client.start();
  tester.waitForServerTerminalOutput("Client 0 connected.\n");
  tester.addClientTerminalInput("test\n");
  tester.waitForClientTerminalOutput("Sending message test\n");
  tester.waitForServerTerminalOutput("Got message: test\n");
  tester.addServerTerminalInput("test\n");
  tester.waitForClientTerminalOutput("Got message: test\n");
  tester.client.stop();
  tester.waitForServerTerminalOutput("Client 0 disconnected.\n");
}


static void testFailureToListen()
{
  TwoServerTester tester;

  tester.server1.start();
  tester.server2.start();
  string expected_output =
    "Unable to start listening: Unable to bind socket.\n";
  tester.waitForServer2TerminalOutput(expected_output);
}


static void testFailureToConnect()
{
  ClientServerTester tester;
  tester.client.start();
  tester.waitForClientTerminalOutput("Connection refused.\n");
}


int main()
{
  testNormalUsage();
  testFailureToListen();
  testFailureToConnect();
}
