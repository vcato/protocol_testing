#include "messageservice.hpp"

using SocketId = SocketsInterface::SocketId;
using std::vector;


bool
  MessageReceiver::receiveMoreOfTheMessage(
    SocketsInterface &sockets,
    MessageHandlerInterface &message_handler,
    SocketId server_socket_id
  )
{
  size_t read_count = chunkSize();
  static const size_t minimum_read_size = 1024;

  if (read_count < minimum_read_size) {
    resizeBuffer(bufferSize() + minimum_read_size - read_count);
    read_count = chunkSize();
  }

  assert(read_count >= minimum_read_size);

  char *chunk_start = chunkStart();
  int read_result = sockets.recv(server_socket_id, chunk_start, read_count);

  if (read_result <= 0) {
    return false;
  }

  if (read_result > 0) {
    size_t n_bytes_read = read_result;
    const void *memchr_result = memchr(chunk_start,'\0',n_bytes_read);
    bool found_null_terminator = !!memchr_result;

    if (found_null_terminator) {
      const char *p = static_cast<const char *>(memchr_result);
      size_t n_message_bytes = p - chunk_start + 1;
      size_t n_extra_bytes = n_bytes_read - n_message_bytes;
      assert(n_bytes_read >= n_message_bytes);
      chunkReceived(n_message_bytes);
      message_handler.gotMessage(bufferStart());
      reset(n_extra_bytes);
      return true;
    }

    chunkReceived(n_bytes_read);
  }

  return true;
}


bool
  MessageSender::sendMoreOfTheMessage(
    SocketsInterface &sockets,
    const SocketId client_socket_id
  )
{
  int send_result = sockets.send(client_socket_id,chunkStart(),chunkSize());

  if (send_result <= 0) {
    return false;
  }

  size_t n_bytes_sent = send_result;

  chunkSent(n_bytes_sent);

  if (nBytesSent() == messageSize()) {
    reset();
  }

  return true;
}


bool
  QueuedMessageSender::handleSendingMessage(
    SocketsInterface &sockets,
    SocketsInterface::SocketId socket_id,
    const PostSelectParamsInterface &post_select_params
  )
{
  assert(message_sender.messageIsBeingSent());

  bool can_send = post_select_params.writeIsSet(socket_id);

  if (!can_send) {
    return true;
  }

  bool could_send =
    message_sender.sendMoreOfTheMessage(sockets,socket_id);

  if (!could_send) {
    return false;
  }

  if (!message_sender.messageIsBeingSent()) {
    message_queue.pop();

    if (!message_queue.empty()) {
      setupNextMessage();
    }
  }

  return true;
}


void QueuedMessageSender::queueMessage(const char *message,int message_size)
{
  message_queue.push(std::vector<char>(message,message + message_size));

  if (!message_sender.messageIsBeingSent()) {
    setupNextMessage();
  }
}


struct MessageServer::MessageHandler : MessageHandlerInterface {
  EventInterface &event_handler;
  const ClientId client_id;

  MessageHandler(EventInterface &event_handler_arg,ClientId client_id_arg)
  : event_handler(event_handler_arg),
    client_id(client_id_arg)
  {
  }

  void gotMessage(const char *message) override
  {
    event_handler.gotMessage(client_id,message);
  }
};


void
  MessageServer::handleReceivingMessage(
    Client &client,
    EventInterface &event_handler,
    ClientId client_id,
    const PostSelectParamsInterface &post_select_params
  )
{
  assert(client.maybe_socket_id);
  SocketId socket_id = *client.maybe_socket_id;

  {
    bool can_recv = post_select_params.readIsSet(socket_id);

    if (!can_recv) {
      return;
    }
  }

  MessageHandler message_handler{event_handler,client_id};

  {
    bool could_receive =
      client.message_receiver.receiveMoreOfTheMessage(
        sockets,message_handler,socket_id
      );

    if (could_receive) {
      return;
    }
  }

  disconnectClient(client_id,event_handler);
}


MessageServer::MessageServer(SocketsInterface &sockets_arg)
: sockets(sockets_arg)
{
}


MessageServer::~MessageServer()
{
  closeSockets();
}


void MessageServer::startListening(int port)
{
  InternetAddress server_address;
  server_address.setPort(port);

  SocketId listen_socket_id = sockets.create();
  sockets.bind(listen_socket_id,server_address);
  sockets.listen(listen_socket_id,/*backlog*/1);
  maybe_listen_socket_id = listen_socket_id;
}


bool MessageServer::isActive() const
{
  if (maybe_listen_socket_id || nClients() != 0) return true;
  return false;
}


void MessageServer::stopListening()
{
  assert(maybe_listen_socket_id);
  sockets.close(*maybe_listen_socket_id);
  maybe_listen_socket_id.reset();
}


bool MessageServer::isSendingAMessageTo(ClientId client_id) const
{
  return isSendingAMessage(clients[client_id]);
}


SocketId MessageServer::clientSocketId(ClientId client_id) const
{
  assert(clients[client_id].maybe_socket_id);
  return *clients[client_id].maybe_socket_id;
}


void MessageServer::acceptConnection(EventInterface &event_handler)
{
  assert(maybe_listen_socket_id);
  size_t client_index = 0;
  size_t n_clients = clients.size();

  for (;;) {
    if (client_index >= n_clients) {
      clients.emplace_back();
    }

    if (!clients[client_index].maybe_socket_id) {
      clients[client_index].maybe_socket_id =
        sockets.accept(*maybe_listen_socket_id);
      event_handler.clientConnected(client_index);
      return;
    }

    ++client_index;
  }
}


void
  MessageServer::setupWaitingForConnection(
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(maybe_listen_socket_id);
  const SocketId listen_socket_id = *maybe_listen_socket_id;
  pre_select_params.setRead(listen_socket_id);
}


void
  MessageServer::handleWaitingForConnection(
    const PostSelectParamsInterface &post_select_params,
    EventInterface &event_handler
  )
{
  assert(maybe_listen_socket_id);
  const SocketId listen_socket_id = *maybe_listen_socket_id;

  if (post_select_params.readIsSet(listen_socket_id)) {
    acceptConnection(event_handler);
  }
}


void
  MessageServer::setupSendingMessage(
    Client &client,
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(isSendingAMessage(client));
  assert(client.maybe_socket_id);
  pre_select_params.setWrite(*client.maybe_socket_id);
}


void
  MessageServer::setupReceivingMessage(
    Client &client,
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(client.maybe_socket_id);
  const SocketId server_socket_id = *client.maybe_socket_id;
  pre_select_params.setRead(server_socket_id);
}


MessageServer::Client &MessageServer::client(ClientId client_id)
{
  assert(clients[client_id].maybe_socket_id);
  return clients[client_id];
}


void
  MessageServer::disconnectClient(
    ClientId client_id,
    EventInterface &event_handler
  )
{
  Client &client = this->client(client_id);
  sockets.close(*client.maybe_socket_id);
  client.maybe_socket_id.reset();
  event_handler.clientDisconnected(client_id);
}


bool
  MessageServer::handleSendingMessage(
    Client &client,
    const PostSelectParamsInterface &post_select_params
  )
{
  assert(client.maybe_socket_id);

  return
    client.queued_message_sender.handleSendingMessage(
      sockets,
      *client.maybe_socket_id,
      post_select_params
    );
}


void MessageServer::setupSelect(PreSelectParamsInterface &pre_select_params)
{
  if (isListening()) {
    setupWaitingForConnection(pre_select_params);
  }

  for (Client &client : clients) {
    if (isConnected(client)) {
      if (isSendingAMessage(client)) {
        setupSendingMessage(client,pre_select_params);
      }

      setupReceivingMessage(client,pre_select_params);
    }
  }
}


void
  MessageServer::queueMessageToClient(
    ClientId client_id,
    const char *message,
    int message_size
  )
{
  client(client_id).queued_message_sender.queueMessage(message,message_size);
}



vector<MessageServer::ClientId> MessageServer::clientIds() const
{
  vector<ClientId> client_ids;
  size_t n = clients.size();

  for (size_t i=0; i!=n; ++i) {
    if (isConnected(clients[i])) {
      client_ids.push_back(i);
    }
  }

  return client_ids;
}


void
  MessageServer::handleSelect(
    const PostSelectParamsInterface &post_select_params,
    EventInterface &event_handler
  )
{
  size_t n = clients.size();

  for (size_t i=0; i!=n; ++i) {
    Client &client = clients[i];

    if (isSendingAMessage(client)) {
      bool could_send = handleSendingMessage(client,post_select_params);

      if (!could_send) {
        disconnectClient(i,event_handler);
      }
    }

    if (isConnected(client)) {
      handleReceivingMessage(client,event_handler,i,post_select_params);
    }
  }

  if (isListening()) {
    handleWaitingForConnection(post_select_params,event_handler);
  }
}


struct MessageClient::MessageHandler : MessageHandlerInterface {
  EventInterface &event_handler;

  MessageHandler(EventInterface &event_handler_arg)
  : event_handler(event_handler_arg)
  {
  }

  void gotMessage(const char *message) override
  {
    event_handler.gotMessage(message);
  }
};


MessageClient::MessageClient(SocketsInterface &sockets_arg)
: sockets(sockets_arg)
{
}


void
  MessageClient::setupReceivingMessage(
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(maybe_socket_id);
  const SocketId client_socket_id = *maybe_socket_id;
  pre_select_params.setRead(client_socket_id);
}


void
  MessageClient::setupSendingMessage(
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(maybe_socket_id);
  pre_select_params.setWrite(*maybe_socket_id);
}


bool MessageClient::isSendingAMessage() const
{
  return queued_message_sender.isSendingAMessage();
}


void
  MessageClient::queueMessage(const char *message_arg,int message_size_arg)
{
  queued_message_sender.queueMessage(message_arg,message_size_arg);
}


bool MessageClient::isActive() const
{
  return !!maybe_socket_id;
}


bool MessageClient::isConnected() const
{
  if (finished_connecting) {
    assert(maybe_socket_id);
    return true;
  }

  return false;
}


void MessageClient::setupSelect(PreSelectParamsInterface &pre_select_params)
{
  if (!isActive()) {
    return;
  }

  if (!finished_connecting) {
    setupWaitingForConnection(pre_select_params);
    return;
  }

  if (isSendingAMessage()) {
    setupSendingMessage(pre_select_params);
  }

  setupReceivingMessage(pre_select_params);
}


void
  MessageClient::handleSelect(
    const PostSelectParamsInterface &post_select_params,
    EventInterface &event_handler
  )
{
  if (!isActive()) return;

  if (!finished_connecting) {
    handleWaitingForConnection(event_handler,post_select_params);
  }
  else if (isSendingAMessage()) {
    handleSendingMessage(post_select_params);
  }
  else {
    handleReceivingMessage(event_handler,post_select_params);
  }
}


void
  MessageClient::handleSendingMessage(
    const PostSelectParamsInterface &post_select_params
  )
{
  assert(maybe_socket_id);
  queued_message_sender.handleSendingMessage(
    sockets,
    *maybe_socket_id,
    post_select_params
  );
}


void
  MessageClient::handleReceivingMessage(
    EventInterface &event_handler,
    const PostSelectParamsInterface &post_select_params
  )
{
  MessageHandler message_handler{event_handler};

  assert(maybe_socket_id);
  SocketId socket_id = *maybe_socket_id;

  {
    bool can_recv = post_select_params.readIsSet(socket_id);

    if (!can_recv) {
      return;
    }
  }

  {
    bool recv_was_successful =
      message_receiver.receiveMoreOfTheMessage(
        sockets,message_handler,socket_id
      );

    if (recv_was_successful) {
      return;
    }
  }

  sockets.close(socket_id);
  maybe_socket_id.reset();
  finished_connecting = false;
}


void MessageClient::disconnect()
{
  assert(finished_connecting);
  assert(maybe_socket_id);
  sockets.close(*maybe_socket_id);
  maybe_socket_id.reset();
  finished_connecting = false;
}


void MessageClient::startConnecting(int port)
{
  assert(!finished_connecting);
  assert(!maybe_socket_id);
  InternetAddress server_address;
  server_address.setHostname("localhost");
  server_address.setPort(port);

  SocketId client_socket_id = sockets.create();
  sockets.setNonBlocking(client_socket_id,true);
  sockets.connect(client_socket_id,server_address);
  maybe_socket_id = client_socket_id;
}


void
  MessageClient::setupWaitingForConnection(
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(maybe_socket_id);
  pre_select_params.setWrite(*maybe_socket_id);
}


void
  MessageClient::handleWaitingForConnection(
    EventInterface &event_handler,
    const PostSelectParamsInterface &post_select_params
  )
{
  assert(maybe_socket_id);
  const SocketId client_socket_id = *maybe_socket_id;
  bool can_send = post_select_params.writeIsSet(client_socket_id);

  if (can_send) {
    if (sockets.connectionWasRefused(client_socket_id)) {
      sockets.close(client_socket_id);
      maybe_socket_id.reset();
      assert(!finished_connecting);
      event_handler.connectionRefused();
      return;
    }

    finished_connecting = true;
  }
}
