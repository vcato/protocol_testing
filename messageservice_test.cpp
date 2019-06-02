#include "messageservice.hpp"

#include <iostream>
#include <sstream>
#include <random>
#include "fakesockets.hpp"
#include "fakeselector.hpp"

using std::deque;
using std::ostream;
using std::ostringstream;
using std::string;
using std::vector;
using std::optional;
using RandomEngine = std::mt19937;
using SocketId = SocketsInterface::SocketId;

static const auto do_nothing = [](auto&&...){};
static const int server_port = 4145;

namespace {
struct TestServer : MessageServer {
  using MessageServer::MessageServer;

  ServerEventCallbacks callbacks;
};
}


namespace {
struct TestClient : MessageClient {
  using MessageClient::MessageClient;

  ClientEventCallbacks callbacks;
};
}


namespace {
struct Tester {
  FakeFileDescriptorAllocator file_descriptor_allocator;
  FakeSockets sockets{file_descriptor_allocator};
  deque<TestServer> servers;
  deque<TestClient> clients;
  FakeSelector selector{{&sockets}};

  TestServer &createServer()
  {
    servers.emplace_back(sockets);
    TestServer &server = servers.back();
    server.startListening(server_port);
    return server;
  }

  void destroyServer(TestServer &server)
  {
    assert(servers.size() == 1);
    assert(&servers[0] == &server);
    servers.clear();
  }

  TestClient &createClient()
  {
    clients.emplace_back(sockets);
    TestClient &client = clients.back();
    client.startConnecting(server_port);
    return client;
  }

  void processEvents()
  {
    selector.beginSelect();

    for (TestServer &server : servers) {
      server.setupSelect(selector.preSelectParams());
    }

    for (TestClient &client : clients) {
      client.setupSelect(selector.preSelectParams());
    }

    selector.callSelect();

    for (TestServer &server : servers) {
      server.handleSelect(selector.postSelectParams(),server.callbacks);
    }

    for (TestClient &client : clients) {
      client.handleSelect(selector.postSelectParams(),client.callbacks);
    }

    selector.endSelect();
  }
};
}


static MessageServer::ClientId
  onlyClientId(const MessageServer &server)
{
  auto client_ids = server.clientIds();
  assert(client_ids.size() == 1);
  return client_ids[0];
}


namespace {
struct ClientServerTester : Tester {
  TestServer &server = createServer();
  TestClient &client = createClient();

  ClientEventCallbacks &clientCallbacks() { return client.callbacks; }
  ServerEventCallbacks &serverCallbacks() { return server.callbacks; }

  MessageServer::ClientId clientId()
  {
    return onlyClientId(server);
  }

  void waitForConnection()
  {
    assert(server.nClients() == 0);

    while (server.nClients() != 1) {
      processEvents();
    }
  }
};
}


static void queueMessageOn(TestClient &client,const char *message)
{
  client.queueMessage(message, strlen(message) + 1);
}


static void
  queueMessageToClientOn(
    TestServer &server,
    MessageServer::ClientId client_id,
    const char *message
  )
{
  server.queueMessageToClient(client_id, message, strlen(message) + 1);
}


static auto reportServerGotMessageFunction(ostream &stream)
{
  return
    [&stream]
    (MessageServer::ClientId client_id, const char *message)
    {
      stream << "gotMessage(" << client_id << "," << message << ")\n";
    };
}


static auto reportServerClientConnectedFunction(ostream &stream)
{
  return
    [&stream]
    (MessageServer::ClientId client_id)
    {
      stream << "clientConnected(" << client_id << ")\n";
    };
}


static auto reportServerClientDisconnectedFunction(ostream &stream)
{
  return
    [&stream]
    (MessageServer::ClientId client_id)
    {
      stream << "clientDisconnected(" << client_id << ")\n";
    };
}


static void testClientSendingAMessage()
{
  ostringstream server_command_stream;
  Tester tester;
  TestServer &server = tester.createServer();
  TestClient &client = tester.createClient();

  auto report_message = reportServerGotMessageFunction(server_command_stream);

  auto report_connect =
    reportServerClientConnectedFunction(server_command_stream);

  auto report_disconnect =
    reportServerClientDisconnectedFunction(server_command_stream);

  server.callbacks.got_message = report_message;
  server.callbacks.client_connected = report_connect;
  server.callbacks.client_disconnected = report_disconnect;

  const char *message = "test2";
  queueMessageOn(client,message);

  while (server.isActive() || client.isActive()) {
    if (client.isActive() && !client.isSendingAMessage()) {
      client.disconnect();
    }

    if (!client.isActive() && server.nClients()==0) {
      server.stopListening();
    }

    tester.processEvents();
  }

  string server_command_string = server_command_stream.str();

  string expected_server_commands =
    "clientConnected(0)\n"
    "gotMessage(0,test2)\n"
    "clientDisconnected(0)\n";

  assert(server_command_string == expected_server_commands);
  assert(tester.sockets.nAllocated() == 0);
}


static void testConnectFailure()
{
  Tester tester;
  ostringstream client_command_stream;
  TestClient &client = tester.createClient();

  client.callbacks.connection_refused =
    [&client_command_stream]{
      client_command_stream << "connectionRefused()\n";
    };

  while (client.isActive()) {
    tester.processEvents();
  }

  assert(tester.sockets.nAllocated() == 0);
  string command_string = client_command_stream.str();
  assert(command_string == "connectionRefused()\n");
}


static void testClientSendingMultipleMessages()
{
  ostringstream server_command_stream;
  Tester tester;
  TestServer &server = tester.createServer();
  TestClient &client = tester.createClient();

  server.callbacks.got_message =
    reportServerGotMessageFunction(server_command_stream);

  server.callbacks.client_connected =
    reportServerClientConnectedFunction(server_command_stream);

  server.callbacks.client_disconnected =
    reportServerClientDisconnectedFunction(server_command_stream);

  int n_messages_queued = 0;

  while (server.isActive() || client.isActive()) {
    if (client.isActive() && !client.isSendingAMessage()) {
      if (n_messages_queued == 0) {
        queueMessageOn(client,"message1");
        ++n_messages_queued;
      }
      else if (n_messages_queued == 1) {
        queueMessageOn(client,"message2");
        ++n_messages_queued;
      }
      else {
        client.disconnect();
      }
    }

    if (!client.isActive() && server.nClients() == 0) {
      server.stopListening();
    }

    tester.processEvents();
  }

  assert(n_messages_queued == 2);
  string server_command_string = server_command_stream.str();

  string expected_command_string =
    "clientConnected(0)\n"
    "gotMessage(0,message1)\n"
    "gotMessage(0,message2)\n"
    "clientDisconnected(0)\n"
    ;

  assert(server_command_string == expected_command_string);
}


static void testServerSendingMessage()
{
  const char *message1 = "message1";
  const char *message2 = "message2";

  vector<string> messages_to_send;
  messages_to_send.push_back(message1);
  messages_to_send.push_back(message2);

  size_t n_messages_sent = 0;
  vector<string> received_messages;

  ostringstream client_command_stream;
  Tester tester;
  TestServer &server = tester.createServer();
  TestClient &client = tester.createClient();

  client.callbacks.got_message =
    [&](const char *message){
      received_messages.push_back(message);
    };
  server.callbacks.client_connected = do_nothing;
  server.callbacks.client_disconnected = do_nothing;

  while (server.isActive() || client.isActive()) {
    if (server.nClients() != 0) {
      MessageServer::ClientId client_id = onlyClientId(server);
      bool should_queue_a_message =
        (n_messages_sent < 2) &&
        !server.isSendingAMessageTo(client_id);

      if (should_queue_a_message) {
        if (n_messages_sent < messages_to_send.size()) {
          const string &message = messages_to_send[n_messages_sent];

          server.queueMessageToClient(
            client_id,
            message.c_str(),
            message.length() + 1
          );
          ++n_messages_sent;
        }
      }
    }

    if (received_messages.size() == 2) {
      if (client.isConnected()) {
        client.disconnect();
      }
    }

    if (!client.isActive() && server.nClients() == 0) {
      server.stopListening();
    }

    tester.processEvents();
  }

  assert(n_messages_sent == 2);
  assert(received_messages == messages_to_send);
}


static void testMultipleClients()
{
  ostringstream server_command_stream;
  Tester tester;

  {
    TestServer &server = tester.createServer();
    TestClient &client1 = tester.createClient();
    TestClient &client2 = tester.createClient();

    server.callbacks.got_message =
      reportServerGotMessageFunction(server_command_stream);
    server.callbacks.client_connected = do_nothing;
    server.callbacks.client_disconnected = do_nothing;

    queueMessageOn(client1,"test1");
    queueMessageOn(client2,"test2");

    while (server.nClients() != 2) {
      tester.processEvents();
    }

    while (server.isActive() || client1.isActive() || client2.isActive()) {
      if (client1.isActive() && !client1.isSendingAMessage()) {
        client1.disconnect();
      }

      if (client2.isActive() && !client2.isSendingAMessage()) {
        client2.disconnect();
      }

      tester.processEvents();

      bool done =
        (!client1.isActive() && !client2.isActive() && server.nClients() == 0);

      if (done) {
        server.stopListening();
      }
    }
  }

  string server_command_string = server_command_stream.str();
  string expected_command_string =
    "gotMessage(0,test1)\n"
    "gotMessage(1,test2)\n";
  assert(server_command_string == expected_command_string);
  assert(tester.sockets.nAllocated() == 0);
}


static void testClientConnectingAfterDisconnect()
{
  ClientServerTester tester;
  MessageServer &server = tester.server;
  MessageClient &client = tester.client;
  int n_connects = 0;
  int n_disconnects = 0;
  tester.serverCallbacks().client_connected =
    [&n_connects](MessageServer::ClientId){ ++n_connects; };
  tester.serverCallbacks().client_disconnected =
    [&n_disconnects](MessageServer::ClientId){ ++n_disconnects; };

  while (server.nClients() != 1) {
    tester.processEvents();
  }

  client.disconnect();

  while (server.nClients() != 0) {
    tester.processEvents();
  }

  client.startConnecting(server_port);

  while (server.nClients() != 1) {
    tester.processEvents();
  }

  client.disconnect();

  while (server.nClients() != 0) {
    tester.processEvents();
  }

  assert(n_connects == 2);
  assert(n_disconnects == 2);
}


static void testDestroyingServerWhileConnected()
{
  Tester tester;
  TestServer &server = tester.createServer();
  TestClient &client1 = tester.createClient();
  TestClient &client2 = tester.createClient();
  server.callbacks.client_connected = do_nothing;

  while (server.nClients() != 2) {
    tester.processEvents();
  }

  tester.destroyServer(server);
  client1.disconnect();
  client2.disconnect();
  assert(tester.sockets.nAllocated() == 0);
}


static void testDestroyingServerWhileConnected2()
{
  Tester tester;
  TestServer &server = tester.createServer();
  TestClient &client1 = tester.createClient();
  TestClient &client2 = tester.createClient();
  server.callbacks.client_connected = do_nothing;

  while (server.nClients() != 2) {
    tester.processEvents();
  }

  tester.destroyServer(server);

  while (client1.isConnected() || client2.isConnected()) {
    tester.processEvents();
  }
}


static void testQueingMultipleMessagesFromServer()
{
  ClientServerTester tester;
  MessageServer &server = tester.server;

  tester.serverCallbacks().client_connected = do_nothing;
  tester.waitForConnection();

  MessageServer::ClientId client_id = onlyClientId(server);

  {
    // Use a local scope here so that we're not relying on the lifetime
    // of the message lasting until it is sent.
    vector<string> messages = {"test1","test2"};

    for (auto &message : messages) {
      server.queueMessageToClient(
        client_id,
        message.c_str(),
        message.length() + 1
      );
    }
  }

  vector<string> received_messages;

  tester.clientCallbacks().got_message =
    [&received_messages](const char *message){
      received_messages.push_back(message);
    };

  while (received_messages.size() != 2) {
    tester.processEvents();
  }

  vector<string> expected_messages = {"test1","test2"};
  assert(received_messages == expected_messages);
}


static void testQueingMultipleMessagesFromClient()
{
  ClientServerTester tester;
  ServerEventCallbacks &server_event_handler = tester.serverCallbacks();
  MessageClient &client = tester.client;

  tester.serverCallbacks().client_connected = do_nothing;
  tester.waitForConnection();

  {
    // Use a local scope here so that we're not relying on the lifetime
    // of the message lasting until it is sent.
    vector<string> messages = {"test1","test2"};

    for (auto &message : messages) {
      client.queueMessage(message.c_str(), message.length() + 1);
    }
  }

  vector<string> received_messages;
  server_event_handler.got_message =
    [&received_messages](MessageServer::ClientId,const char *message){
      received_messages.push_back(message);
    };

  while (received_messages.size() != 2) {
    tester.processEvents();
  }

  vector<string> expected_messages = {"test1","test2"};
  assert(received_messages == expected_messages);
}


static string randomMessageOfLength(size_t n,RandomEngine &engine)
{
  string message;
  message.resize(n);
  auto min_char = 1;
  auto max_char = std::numeric_limits<char>::max();
  auto distribution = std::uniform_int_distribution<char>(min_char,max_char);
  auto random_char_function = [&]{ return distribution(engine); };
  std::generate(message.begin(),message.end(),random_char_function);
  return message;
}


static void testSendingLongMessage()
{
  ClientServerTester tester;
  TestServer &server = tester.server;
  TestClient &client = tester.client;

  server.callbacks.client_connected = do_nothing;

  tester.waitForConnection();
  RandomEngine engine;
  engine.seed(1);

  string sent_message = randomMessageOfLength(100000,engine);
  queueMessageOn(client,sent_message.c_str());

  optional<string> maybe_received_message;

  server.callbacks.got_message =
    [&](MessageServer::ClientId,const char *message){
      maybe_received_message.emplace(message);
    };

  while (!maybe_received_message) {
    tester.processEvents();
  }

  const string &received_message = *maybe_received_message;
  assert(received_message == sent_message);
}


static void testReadError()
{
  ClientServerTester tester;
  tester.server.callbacks.client_connected = do_nothing;
  tester.server.callbacks.client_disconnected = do_nothing;
  tester.waitForConnection();
  queueMessageOn(tester.client,"this is a message");
  // Set the number of bytes we can read before getting an error.
  SocketId server_client_socket_id =
    tester.server.clientSocketId(tester.clientId());
  tester.sockets.setNBytesBeforeRecvError(server_client_socket_id, 10);

  assert(tester.server.nClients() == 1);

  // The server should get a read error and disconnect the client.
  while (tester.server.nClients() != 0) {
    tester.processEvents();
  }
}


static void testSendEOF()
{
  ClientServerTester tester;
  tester.server.callbacks.client_connected = do_nothing;
  tester.server.callbacks.client_disconnected = do_nothing;
  tester.waitForConnection();
  queueMessageToClientOn(tester.server, tester.clientId(), "this is a message");
  tester.client.disconnect();

  while (tester.server.nClients() != 0) {
    tester.processEvents();
  }
}


static void testSendError()
{
  ClientServerTester tester;
  TestServer &server = tester.server;
  server.callbacks.client_connected = do_nothing;
  server.callbacks.client_disconnected = do_nothing;
  tester.waitForConnection();
  MessageServer::ClientId client_id = tester.clientId();
  queueMessageToClientOn(server, client_id, "this is a message");
  tester.sockets.setNBytesBeforeSendError(server.clientSocketId(client_id), 10);

  while (server.nClients() != 0) {
    tester.processEvents();
  }
}


static void runUnitTests()
{
  testClientSendingAMessage();
  testConnectFailure();
  testClientSendingMultipleMessages();
  testServerSendingMessage();
  testMultipleClients();
  testClientConnectingAfterDisconnect();
  testDestroyingServerWhileConnected();
  testDestroyingServerWhileConnected2();
  testQueingMultipleMessagesFromServer();
  testQueingMultipleMessagesFromClient();
  testSendingLongMessage();
  testReadError();
  testSendEOF();
  testSendError();
}

int main()
{
  runUnitTests();
}
