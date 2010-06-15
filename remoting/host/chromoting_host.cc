// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host.h"

#include "base/stl_util-inl.h"
#include "base/waitable_event.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/host/host_config.h"
#include "remoting/host/session_manager.h"
#include "remoting/jingle_glue/jingle_channel.h"

namespace remoting {

ChromotingHost::ChromotingHost(HostConfig* config,
                               Capturer* capturer,
                               Encoder* encoder,
                               EventExecutor* executor,
                               base::WaitableEvent* host_done)
      : main_thread_("MainThread"),
        capture_thread_("CaptureThread"),
        encode_thread_("EncodeThread"),
        config_(config),
        capturer_(capturer),
        encoder_(encoder),
        executor_(executor),
        host_done_(host_done) {
  // TODO(ajwong): The thread injection and object ownership is odd here.
  // Fix so we do not start this thread in the constructor, so we only
  // take in a session manager, don't let session manager own the
  // capturer/encoder, and then associate the capturer and encoder threads with
  // the capturer and encoder objects directly.  This will require a
  // non-refcounted NewRunnableMethod.
  main_thread_.StartWithOptions(
      base::Thread::Options(MessageLoop::TYPE_UI, 0));
  network_thread_.Start();
}

ChromotingHost::~ChromotingHost() {
  // TODO(ajwong): We really need to inject these threads and get rid of these
  // start/stops.
  main_thread_.Stop();
  network_thread_.Stop();
  DCHECK(!encode_thread_.IsRunning());
  DCHECK(!capture_thread_.IsRunning());
}

void ChromotingHost::Run() {
  // Submit a task to perform host registration. We'll also start
  // listening to connection if registration is done.
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ChromotingHost::RegisterHost));
}

// This method is called when we need to destroy the host process.
void ChromotingHost::DestroySession() {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // First we tell the session to pause and then we wait until all
  // the tasks are done.
  if (session_.get()) {
    session_->Pause();

    // TODO(hclam): Revise the order.
    DCHECK(encode_thread_.IsRunning());
    encode_thread_.Stop();

    DCHECK(capture_thread_.IsRunning());
    capture_thread_.Stop();
  }
}

// This method talks to the cloud to register the host process. If
// successful we will start listening to network requests.
void ChromotingHost::RegisterHost() {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(!jingle_client_);

  // Connect to the talk network with a JingleClient.
  jingle_client_ = new JingleClient(&network_thread_);
  jingle_client_->Init(config_->xmpp_login(), config_->xmpp_auth_token(),
                       kChromotingTokenServiceName, this);
}

// This method is called if a client is connected to this object.
void ChromotingHost::OnClientConnected(ClientConnection* client) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Create a new RecordSession if there was none.
  if (!session_.get()) {
    // The first we need to make sure capture and encode thread are
    // running.
    capture_thread_.Start();
    encode_thread_.Start();

    // Then we create a SessionManager passing the message loops that
    // it should run on.
    // Note that we pass the ownership of the capturer and encoder to
    // the session manager.
    DCHECK(capturer_.get());
    DCHECK(encoder_.get());
    session_ = new SessionManager(capture_thread_.message_loop(),
                                  encode_thread_.message_loop(),
                                  message_loop(),
                                  capturer_.release(),
                                  encoder_.release());

    // Immediately add the client and start the session.
    session_->AddClient(client);
    session_->Start();
    LOG(INFO) << "Session manager started";
  } else {
    // If a session manager already exists we simply add the new client.
    session_->AddClient(client);
  }
}

void ChromotingHost::OnClientDisconnected(ClientConnection* client) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Remove the client from the session manager.
  if (session_.get())
    session_->RemoveClient(client);

  // Also remove reference to ClientConnection from this object.
  client_ = NULL;

  // TODO(hclam): If the last client has disconnected we need to destroy
  // the session manager and shutdown the capture and encode threads.
  // Right now we assume that there's only one client.
  DestroySession();
}

////////////////////////////////////////////////////////////////////////////
// ClientConnection::EventHandler implementations
void ChromotingHost::HandleMessages(ClientConnection* client,
                                ClientMessageList* messages) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Delegate the messages to EventExecutor and delete the unhandled
  // messages.
  DCHECK(executor_.get());
  executor_->HandleInputEvents(messages);
  STLDeleteElements<ClientMessageList>(messages);
}

void ChromotingHost::OnConnectionOpened(ClientConnection* client) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Completes the client connection.
  LOG(INFO) << "Connection to client established.";
  OnClientConnected(client_.get());
}

void ChromotingHost::OnConnectionClosed(ClientConnection* client) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Completes the client connection.
  LOG(INFO) << "Connection to client closed.";
  OnClientDisconnected(client_.get());
}

void ChromotingHost::OnConnectionFailed(ClientConnection* client) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // The client has disconnected.
  LOG(ERROR) << "Connection failed unexpectedly.";
  OnClientDisconnected(client_.get());
}

////////////////////////////////////////////////////////////////////////////
// JingleClient::Callback implementations
void ChromotingHost::OnStateChange(JingleClient* jingle_client,
                               JingleClient::State state) {
  DCHECK_EQ(jingle_client_.get(), jingle_client);

  if (state == JingleClient::CONNECTED) {
    LOG(INFO) << "Host connected as "
              << jingle_client->GetFullJid() << "." << std::endl;

    // Start heartbeating after we connected
    heartbeat_sender_ = new HeartbeatSender();
    // TODO(sergeyu): where do we get host id?
    heartbeat_sender_->Start(config_, jingle_client_.get());
  } else if (state == JingleClient::CLOSED) {
    LOG(INFO) << "Host disconnected from talk network." << std::endl;
    heartbeat_sender_ = NULL;

    // Quit the message loop if disconected.
    message_loop()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    host_done_->Signal();
  }
}

bool ChromotingHost::OnAcceptConnection(
    JingleClient* jingle_client, const std::string& jid,
    JingleChannel::Callback** channel_callback) {
  DCHECK_EQ(jingle_client_.get(), jingle_client);

  // TODO(hclam): Allow multiple clients to connect to the host.
  if (client_.get())
    return false;

  LOG(INFO) << "Client connected: " << jid << std::endl;

  // If we accept the connected then create a client object and set the
  // callback.
  client_ = new ClientConnection(message_loop(), new ProtocolDecoder(), this);
  *channel_callback = client_.get();
  return true;
}

void ChromotingHost::OnNewConnection(JingleClient* jingle_client,
                                 scoped_refptr<JingleChannel> channel) {
  DCHECK_EQ(jingle_client_.get(), jingle_client);

  // Since the session manager has not started, it is still safe to access
  // the client directly. Note that we give the ownership of the channel
  // to the client.
  client_->set_jingle_channel(channel);
}

MessageLoop* ChromotingHost::message_loop() {
  return main_thread_.message_loop();
}

}  // namespace remoting
