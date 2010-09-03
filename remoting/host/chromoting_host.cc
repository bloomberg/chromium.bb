// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host.h"

#include "base/stl_util-inl.h"
#include "base/task.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/base/encoder.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/capturer.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/host_config.h"
#include "remoting/host/session_manager.h"
#include "remoting/jingle_glue/jingle_channel.h"

namespace remoting {

ChromotingHost::ChromotingHost(ChromotingHostContext* context,
                               MutableHostConfig* config,
                               Capturer* capturer,
                               Encoder* encoder,
                               EventExecutor* executor)
    : context_(context),
      config_(config),
      capturer_(capturer),
      encoder_(encoder),
      executor_(executor),
      state_(kInitial) {
}

ChromotingHost::~ChromotingHost() {
}

void ChromotingHost::Start(Task* shutdown_task) {
  // Get capturer to set up it's initial configuration.
  capturer_->ScreenConfigurationChanged();

  // Submit a task to perform host registration. We'll also start
  // listening to connection if registration is done.
  context_->main_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ChromotingHost::DoStart, shutdown_task));
}

// This method is called when we need to destroy the host process.
void ChromotingHost::Shutdown() {
  context_->main_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ChromotingHost::DoShutdown));
}

// This method is called if a client is connected to this object.
void ChromotingHost::OnClientConnected(ClientConnection* client) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  // Create a new RecordSession if there was none.
  if (!session_.get()) {
    // Then we create a SessionManager passing the message loops that
    // it should run on.
    DCHECK(capturer_.get());
    DCHECK(encoder_.get());
    session_ = new SessionManager(context_->capture_message_loop(),
                                  context_->encode_message_loop(),
                                  context_->main_message_loop(),
                                  capturer_.get(),
                                  encoder_.get());
  }

  // Immediately add the client and start the session.
  session_->AddClient(client);
  session_->Start();
  LOG(INFO) << "Session manager started";
}

void ChromotingHost::OnClientDisconnected(ClientConnection* client) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  // Remove the client from the session manager and pause the session.
  // TODO(hclam): Pause only if the last client disconnected.
  if (session_.get()) {
    session_->RemoveClient(client);
    session_->Pause();
  }

  // Close the connection to client just to be safe.
  client->Disconnect();

  // Also remove reference to ClientConnection from this object.
  client_ = NULL;
}

////////////////////////////////////////////////////////////////////////////
// ClientConnection::EventHandler implementations
void ChromotingHost::HandleMessages(ClientConnection* client,
                                    ClientMessageList* messages) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  // Delegate the messages to EventExecutor and delete the unhandled
  // messages.
  DCHECK(executor_.get());
  executor_->HandleInputEvents(messages);
  STLDeleteElements<ClientMessageList>(messages);
}

void ChromotingHost::OnConnectionOpened(ClientConnection* client) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  // Completes the client connection.
  LOG(INFO) << "Connection to client established.";
  OnClientConnected(client_.get());
}

void ChromotingHost::OnConnectionClosed(ClientConnection* client) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  LOG(INFO) << "Connection to client closed.";
  OnClientDisconnected(client_.get());
}

void ChromotingHost::OnConnectionFailed(ClientConnection* client) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  LOG(ERROR) << "Connection failed unexpectedly.";
  OnClientDisconnected(client_.get());
}

////////////////////////////////////////////////////////////////////////////
// JingleClient::Callback implementations
void ChromotingHost::OnStateChange(JingleClient* jingle_client,
                                   JingleClient::State state) {
  if (state == JingleClient::CONNECTED) {
    DCHECK_EQ(jingle_client_.get(), jingle_client);
    LOG(INFO) << "Host connected as "
              << jingle_client->GetFullJid() << "." << std::endl;

    // Start heartbeating after we've connected.
    heartbeat_sender_->Start();
  } else if (state == JingleClient::CLOSED) {
    LOG(INFO) << "Host disconnected from talk network." << std::endl;

    // Stop heartbeating.
    heartbeat_sender_->Stop();

    // TODO(sergeyu): We should try reconnecting here instead of terminating
    // the host.
    // Post a shutdown task to properly shutdown the chromoting host.
    context_->main_message_loop()->PostTask(
        FROM_HERE, NewRunnableMethod(this, &ChromotingHost::DoShutdown));
  }
}

bool ChromotingHost::OnAcceptConnection(
    JingleClient* jingle_client, const std::string& jid,
    JingleChannel::Callback** channel_callback) {
  AutoLock auto_lock(lock_);
  if (state_ != kStarted)
    return false;

  DCHECK_EQ(jingle_client_.get(), jingle_client);

  // TODO(hclam): Allow multiple clients to connect to the host.
  if (client_.get())
    return false;

  // Check that the user has access to the host.
  if (!access_verifier_.VerifyPermissions(jid))
    return false;

  LOG(INFO) << "Client connected: " << jid << std::endl;

  // If we accept the connected then create a client object and set the
  // callback.
  client_ = new ClientConnection(context_->main_message_loop(),
                                 new ProtocolDecoder(), this);
  *channel_callback = client_.get();
  return true;
}

void ChromotingHost::OnNewConnection(JingleClient* jingle_client,
                                     scoped_refptr<JingleChannel> channel) {
  AutoLock auto_lock(lock_);
  if (state_ != kStarted)
    return;

  DCHECK_EQ(jingle_client_.get(), jingle_client);

  // Since the session manager has not started, it is still safe to access
  // the client directly. Note that we give the ownership of the channel
  // to the client.
  client_->set_jingle_channel(channel);
}

void ChromotingHost::DoStart(Task* shutdown_task) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());
  DCHECK(!jingle_client_);
  DCHECK(shutdown_task);

  // Make sure this object is not started.
  {
    AutoLock auto_lock(lock_);
    if (state_ != kInitial)
      return;
    state_ = kStarted;
  }

  // Save the shutdown task.
  shutdown_task_.reset(shutdown_task);

  std::string xmpp_login;
  std::string xmpp_auth_token;
  if (!config_->GetString(kXmppLoginConfigPath, &xmpp_login) ||
      !config_->GetString(kXmppAuthTokenConfigPath, &xmpp_auth_token)) {
    LOG(ERROR) << "XMPP credentials are not defined in the config.";
    return;
  }

  if (!access_verifier_.Init(config_))
    return;

  // Connect to the talk network with a JingleClient.
  jingle_client_ = new JingleClient(context_->jingle_thread());
  jingle_client_->Init(xmpp_login, xmpp_auth_token,
                       kChromotingTokenServiceName, this);

  heartbeat_sender_ = new HeartbeatSender();
  if (!heartbeat_sender_->Init(config_, jingle_client_.get())) {
    LOG(ERROR) << "Failed to initialize HeartbeatSender.";
    return;
  }
}

void ChromotingHost::DoShutdown() {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  // No-op if this object is not started yet.
  {
    AutoLock auto_lock(lock_);
    if (state_ != kStarted)
      return;
    state_ = kStopped;
  }

  // Tell the session to pause and then disconnect all clients.
  if (session_.get()) {
    session_->Pause();
    session_->RemoveAllClients();
  }

  // Disconnect all clients.
  if (client_) {
    client_->Disconnect();
  }

  // Disconnect from the talk network.
  if (jingle_client_) {
    jingle_client_->Close();
  }

  // Stop the heartbeat sender.
  if (heartbeat_sender_) {
    heartbeat_sender_->Stop();
  }

  // Lastly call the shutdown task.
  if (shutdown_task_.get()) {
    shutdown_task_->Run();
  }
}

}  // namespace remoting
