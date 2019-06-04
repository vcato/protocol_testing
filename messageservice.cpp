#include "messageservice.hpp"

#include <string.h>
#include <cassert>

using SocketId = SocketsInterface::SocketId;
using std::vector;


struct MessageReceiver::Impl {
  static size_t bufferSize(const MessageReceiver &self)
  {
    return self.buffer.size();
  }

  static size_t chunkSize(const MessageReceiver &self)
  {
    return bufferSize(self) - self.n_bytes_read;
  }

  static void chunkReceived(MessageReceiver &self,size_t n)
  {
    assert(n <= chunkSize(self));
    self.n_bytes_read += n;
    assert(self.n_bytes_read <= bufferSize(self));
  }

  static void reset(MessageReceiver &self,size_t n_bytes_to_keep)
  {
    assert(n_bytes_to_keep <= chunkSize(self));
    memmove(self.buffer.data(), chunkStart(self), n_bytes_to_keep);
    self.n_bytes_read = n_bytes_to_keep;
  }

  static char *chunkStart(MessageReceiver &self)
  {
    return self.buffer.data() + self.n_bytes_read;
  }

  static const char *bufferStart(const MessageReceiver &self)
  {
    return self.buffer.data();
  }

  static void resizeBuffer(MessageReceiver &self,size_t new_size)
  {
    assert(new_size >= self.n_bytes_read);
    self.buffer.resize(new_size);
  }
};


bool
  MessageReceiver::receiveMoreOfTheMessage(
    SocketsInterface &sockets,
    EventInterface &message_handler,
    SocketId server_socket_id
  )
{
  size_t read_count = Impl::chunkSize(*this);
  static const size_t minimum_read_size = 1024;

  if (read_count < minimum_read_size) {
    size_t n_bytes_needed = minimum_read_size - read_count;
    Impl::resizeBuffer(*this, Impl::bufferSize(*this) + n_bytes_needed);
    read_count = Impl::chunkSize(*this);
  }

  assert(read_count >= minimum_read_size);

  char *chunk_start = Impl::chunkStart(*this);
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
      Impl::chunkReceived(*this,n_message_bytes);
      message_handler.gotMessage(Impl::bufferStart(*this));
      Impl::reset(*this,n_extra_bytes);
      return true;
    }

    Impl::chunkReceived(*this,n_bytes_read);
  }

  return true;
}


struct MessageSender::Impl {
  static size_t nBytesSent(const MessageSender &self)
  {
    return self.n_bytes_sent;
  }

  static size_t messageSize(const MessageSender &self)
  {
    return self.message_size;
  }

  static void reset(MessageSender &self)
  {
    self.message_being_sent = nullptr;
  }

  static const char *chunkStart(const MessageSender &self)
  {
    return self.message_being_sent + self.n_bytes_sent;
  }

  static size_t chunkSize(const MessageSender &self)
  {
    return self.message_size - self.n_bytes_sent;
  }

  static void chunkSent(MessageSender &self,size_t n)
  {
    assert(n <= chunkSize(self));
    self.n_bytes_sent += n;
    assert(self.n_bytes_sent <= self.message_size);
  }
};


void MessageSender::queueMessage(const char *message_arg, int message_size_arg)
{
  assert(!message_being_sent);
  message_being_sent = message_arg;
  message_size = message_size_arg;
  n_bytes_sent = 0;
}


bool
  MessageSender::sendMoreOfTheMessage(
    SocketsInterface &sockets,
    const SocketId client_socket_id
  )
{
  const char *chunk_start = Impl::chunkStart(*this);
  size_t chunk_size = Impl::chunkSize(*this);

  int send_result = sockets.send(client_socket_id,chunk_start,chunk_size);

  if (send_result <= 0) {
    return false;
  }

  size_t n_bytes_sent = send_result;

  Impl::chunkSent(*this,n_bytes_sent);

  if (Impl::nBytesSent(*this) == Impl::messageSize(*this)) {
    Impl::reset(*this);
  }

  return true;
}


struct QueuedMessageSender::Impl {
  static void setupNextMessage(QueuedMessageSender &self)
  {
    std::vector<char> &message = self.message_queue.front();
    self.message_sender.queueMessage(message.data(), message.size());
  }
};


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
      Impl::setupNextMessage(*this);
    }
  }

  return true;
}


void QueuedMessageSender::queueMessage(const char *message,int message_size)
{
  message_queue.push(std::vector<char>(message,message + message_size));

  if (!message_sender.messageIsBeingSent()) {
    Impl::setupNextMessage(*this);
  }
}


struct MessageServer::Client {
  Client() = default;
  Client(Client &&) = default;
  std::optional<SocketId> maybe_socket_id;
  MessageReceiver message_receiver;
  QueuedMessageSender queued_message_sender;
};


struct MessageServer::Impl {
  struct MessageHandler : MessageReceiver::EventInterface {
    MessageServer::EventInterface &event_handler;
    const ClientId client_id;

    MessageHandler(
      MessageServer::EventInterface &event_handler_arg,
      ClientId client_id_arg
    )
    : event_handler(event_handler_arg),
      client_id(client_id_arg)
    {
    }

    void gotMessage(const char *message) override
    {
      event_handler.gotMessage(client_id,message);
    }
  };

  static bool isListening(const MessageServer &self)
  {
    return self.maybe_listen_socket_id.has_value();
  }

  static void acceptConnection(MessageServer &self,EventInterface &);

  static void
    setupWaitingForConnection(MessageServer &self,PreSelectParamsInterface &);

  static void
    handleWaitingForConnection(
      MessageServer &self,
      const PostSelectParamsInterface &,
      EventInterface &
    );

  static void closeClientSockets(MessageServer &self)
  {
    std::vector<Client> &clients = self.clients;
    size_t n_clients = clients.size();

    for (size_t i=0; i!=n_clients; ++i) {
      if (clients[i].maybe_socket_id) {
        self.sockets.close(*clients[i].maybe_socket_id);
        clients[i].maybe_socket_id.reset();
      }
    }
  }

  static void closeListenSocket(MessageServer &self)
  {
    if (self.maybe_listen_socket_id) {
      self.sockets.close(*self.maybe_listen_socket_id);
      self.maybe_listen_socket_id.reset();
    }
  }

  static void closeSockets(MessageServer &self)
  {
    closeClientSockets(self);
    closeListenSocket(self);
  }

  static bool isSendingAMessage(const Client &client)
  {
    return client.queued_message_sender.isSendingAMessage();
  }

  static bool isConnected(const Client &client)
  {
    return client.maybe_socket_id.has_value();
  }

  static void setupReceivingMessage(Client &,PreSelectParamsInterface &);
  static void setupSendingMessage(Client &,PreSelectParamsInterface &);

  static void
    handleReceivingMessage(
      MessageServer &self,
      Client &,
      EventInterface &,
      ClientId,
      const PostSelectParamsInterface &
    );

  static bool
    handleSendingMessage(
      MessageServer &self,
      Client &,
      const PostSelectParamsInterface &
    );

  static void
    disconnectClient(
      MessageServer &self,
      ClientId,
      EventInterface &event_handler
    );

  static Client &client(MessageServer &self,ClientId);
};


void
  MessageServer::Impl::handleReceivingMessage(
    MessageServer &self,
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
        self.sockets,message_handler,socket_id
      );

    if (could_receive) {
      return;
    }
  }

  disconnectClient(self,client_id,event_handler);
}


MessageServer::MessageServer(SocketsInterface &sockets_arg)
: sockets(sockets_arg)
{
}


MessageServer::~MessageServer()
{
  Impl::closeSockets(*this);
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
  return Impl::isSendingAMessage(clients[client_id]);
}


SocketId MessageServer::clientSocketId(ClientId client_id) const
{
  assert(clients[client_id].maybe_socket_id);
  return *clients[client_id].maybe_socket_id;
}


void
  MessageServer::Impl::acceptConnection(
    MessageServer &self,
    EventInterface &event_handler
  )
{
  assert(self.maybe_listen_socket_id);
  const SocketId listen_socket_id = *self.maybe_listen_socket_id;
  size_t client_index = 0;
  vector<Client> &clients = self.clients;
  size_t n_clients = clients.size();

  for (;;) {
    if (client_index >= n_clients) {
      clients.emplace_back();
    }

    if (!clients[client_index].maybe_socket_id) {
      clients[client_index].maybe_socket_id =
        self.sockets.accept(listen_socket_id);
      event_handler.clientConnected(client_index);
      return;
    }

    ++client_index;
  }
}


void
  MessageServer::Impl::setupWaitingForConnection(
    MessageServer &self,
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(self.maybe_listen_socket_id);
  const SocketId listen_socket_id = *self.maybe_listen_socket_id;
  pre_select_params.setRead(listen_socket_id);
}


void
  MessageServer::Impl::handleWaitingForConnection(
    MessageServer &self,
    const PostSelectParamsInterface &post_select_params,
    EventInterface &event_handler
  )
{
  assert(self.maybe_listen_socket_id);
  const SocketId listen_socket_id = *self.maybe_listen_socket_id;

  if (post_select_params.readIsSet(listen_socket_id)) {
    acceptConnection(self,event_handler);
  }
}


void
  MessageServer::Impl::setupSendingMessage(
    Client &client,
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(isSendingAMessage(client));
  assert(client.maybe_socket_id);
  pre_select_params.setWrite(*client.maybe_socket_id);
}


void
  MessageServer::Impl::setupReceivingMessage(
    Client &client,
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(client.maybe_socket_id);
  const SocketId server_socket_id = *client.maybe_socket_id;
  pre_select_params.setRead(server_socket_id);
}


MessageServer::Client &
  MessageServer::Impl::client(MessageServer &self,ClientId client_id)
{
  assert(self.clients[client_id].maybe_socket_id);
  return self.clients[client_id];
}


void
  MessageServer::Impl::disconnectClient(
    MessageServer &self,
    ClientId client_id,
    EventInterface &event_handler
  )
{
  Client &client = Impl::client(self,client_id);
  self.sockets.close(*client.maybe_socket_id);
  client.maybe_socket_id.reset();
  event_handler.clientDisconnected(client_id);
}


bool
  MessageServer::Impl::handleSendingMessage(
    MessageServer &self,
    Client &client,
    const PostSelectParamsInterface &post_select_params
  )
{
  assert(client.maybe_socket_id);

  return
    client.queued_message_sender.handleSendingMessage(
      self.sockets,
      *client.maybe_socket_id,
      post_select_params
    );
}


void MessageServer::setupSelect(PreSelectParamsInterface &pre_select_params)
{
  if (Impl::isListening(*this)) {
    Impl::setupWaitingForConnection(*this,pre_select_params);
  }

  for (Client &client : clients) {
    if (Impl::isConnected(client)) {
      if (Impl::isSendingAMessage(client)) {
        Impl::setupSendingMessage(client,pre_select_params);
      }

      Impl::setupReceivingMessage(client,pre_select_params);
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
  Client &client = MessageServer::Impl::client(*this,client_id);
  client.queued_message_sender.queueMessage(message,message_size);
}



vector<MessageServer::ClientId> MessageServer::clientIds() const
{
  vector<ClientId> client_ids;
  size_t n = clients.size();

  for (size_t i=0; i!=n; ++i) {
    if (Impl::isConnected(clients[i])) {
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

    if (Impl::isSendingAMessage(client)) {
      bool could_send =
        Impl::handleSendingMessage(*this,client,post_select_params);

      if (!could_send) {
        Impl::disconnectClient(*this,i,event_handler);
      }
    }

    if (Impl::isConnected(client)) {
      Impl::handleReceivingMessage(
        *this,client,event_handler,i,post_select_params
      );
    }
  }

  if (Impl::isListening(*this)) {
    Impl::handleWaitingForConnection(*this,post_select_params,event_handler);
  }
}


struct MessageClient::Impl {
  struct MessageHandler : MessageReceiver::EventInterface {
    MessageClient::EventInterface &event_handler;

    MessageHandler(MessageClient::EventInterface &event_handler_arg)
    : event_handler(event_handler_arg)
    {
    }

    void gotMessage(const char *message) override
    {
      event_handler.gotMessage(message);
    }
  };

  static void
    setupWaitingForConnection(MessageClient &,PreSelectParamsInterface &);

  static void
    handleWaitingForConnection(
      MessageClient &,
      EventInterface &,
      const PostSelectParamsInterface &
    );

  static void
    setupReceivingMessage(MessageClient &,PreSelectParamsInterface &);

  static void setupSendingMessage(MessageClient &,PreSelectParamsInterface &);

  static void
    handleSendingMessage(MessageClient &,const PostSelectParamsInterface &);

  static void
    handleReceivingMessage(
      MessageClient &,
      EventInterface &,
      const PostSelectParamsInterface &
    );
};


MessageClient::MessageClient(SocketsInterface &sockets_arg)
: sockets(sockets_arg)
{
}


void
  MessageClient::Impl::setupReceivingMessage(
    MessageClient &self,
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(self.maybe_socket_id);
  const SocketId client_socket_id = *self.maybe_socket_id;
  pre_select_params.setRead(client_socket_id);
}


void
  MessageClient::Impl::setupSendingMessage(
    MessageClient &self,
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(self.maybe_socket_id);
  pre_select_params.setWrite(*self.maybe_socket_id);
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
    Impl::setupWaitingForConnection(*this,pre_select_params);
    return;
  }

  if (isSendingAMessage()) {
    Impl::setupSendingMessage(*this,pre_select_params);
  }

  Impl::setupReceivingMessage(*this,pre_select_params);
}


void
  MessageClient::handleSelect(
    const PostSelectParamsInterface &post_select_params,
    EventInterface &event_handler
  )
{
  if (!isActive()) return;

  if (!finished_connecting) {
    Impl::handleWaitingForConnection(*this,event_handler,post_select_params);
  }
  else if (isSendingAMessage()) {
    Impl::handleSendingMessage(*this,post_select_params);
  }
  else {
    Impl::handleReceivingMessage(*this,event_handler,post_select_params);
  }
}


void
  MessageClient::Impl::handleSendingMessage(
    MessageClient &self,
    const PostSelectParamsInterface &post_select_params
  )
{
  assert(self.maybe_socket_id);
  self.queued_message_sender.handleSendingMessage(
    self.sockets,
    *self.maybe_socket_id,
    post_select_params
  );
}


void
  MessageClient::Impl::handleReceivingMessage(
    MessageClient &self,
    EventInterface &event_handler,
    const PostSelectParamsInterface &post_select_params
  )
{
  MessageHandler message_handler{event_handler};

  assert(self.maybe_socket_id);
  SocketId socket_id = *self.maybe_socket_id;

  {
    bool can_recv = post_select_params.readIsSet(socket_id);

    if (!can_recv) {
      return;
    }
  }

  {
    bool recv_was_successful =
      self.message_receiver.receiveMoreOfTheMessage(
        self.sockets,message_handler,socket_id
      );

    if (recv_was_successful) {
      return;
    }
  }

  self.sockets.close(socket_id);
  self.maybe_socket_id.reset();
  self.finished_connecting = false;
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
  MessageClient::Impl::setupWaitingForConnection(
    MessageClient &self,
    PreSelectParamsInterface &pre_select_params
  )
{
  assert(self.maybe_socket_id);
  pre_select_params.setWrite(*self.maybe_socket_id);
}


void
  MessageClient::Impl::handleWaitingForConnection(
    MessageClient &self,
    EventInterface &event_handler,
    const PostSelectParamsInterface &post_select_params
  )
{
  assert(self.maybe_socket_id);
  const SocketId client_socket_id = *self.maybe_socket_id;
  bool can_send = post_select_params.writeIsSet(client_socket_id);

  if (can_send) {
    if (self.sockets.connectionWasRefused(client_socket_id)) {
      self.sockets.close(client_socket_id);
      self.maybe_socket_id.reset();
      assert(!self.finished_connecting);
      event_handler.connectionRefused();
      return;
    }

    event_handler.connected();
    self.finished_connecting = true;
  }
}
