// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host.h"

#include "base/stl_util-inl.h"
#include "base/task.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/base/encoder.h"
#include "remoting/base/encoder_verbatim.h"
#include "remoting/base/encoder_vp8.h"
#include "remoting/base/encoder_zlib.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/capturer.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/host_config.h"
#include "remoting/host/session_manager.h"
#include "remoting/protocol/jingle_chromotocol_server.h"
#include "remoting/protocol/chromotocol_config.h"

namespace remoting {

ChromotingHost::ChromotingHost(ChromotingHostContext* context,
                               MutableHostConfig* config,
                               Capturer* capturer,
                               EventExecutor* executor)
    : context_(context),
      config_(config),
      capturer_(capturer),
      executor_(executor),
      state_(kInitial) {
}

ChromotingHost::~ChromotingHost() {
}

void ChromotingHost::Start(Task* shutdown_task) {
  if (MessageLoop::current() != context_->main_message_loop()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingHost::Start, shutdown_task));
    return;
  }

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

// This method is called when we need to destroy the host process.
void ChromotingHost::Shutdown() {
  if (MessageLoop::current() != context_->main_message_loop()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingHost::Shutdown));
    return;
  }

  // No-op if this object is not started yet.
  {
    AutoLock auto_lock(lock_);
    if (state_ != kStarted) {
      state_ = kStopped;
      return;
    }
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

  // Stop the heartbeat sender.
  if (heartbeat_sender_) {
    heartbeat_sender_->Stop();
  }

  // Stop chromotocol server.
  if (chromotocol_server_) {
    chromotocol_server_->Close(
        NewRunnableMethod(this, &ChromotingHost::OnServerClosed));
  }

  // Disconnect from the talk network.
  if (jingle_client_) {
    jingle_client_->Close();
  }

  // Lastly call the shutdown task.
  if (shutdown_task_.get()) {
    shutdown_task_->Run();
  }
}

// This method is called if a client is connected to this object.
void ChromotingHost::OnClientConnected(ClientConnection* client) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  // Create a new RecordSession if there was none.
  if (!session_.get()) {
    // Then we create a SessionManager passing the message loops that
    // it should run on.
    DCHECK(capturer_.get());

    Encoder* encoder = CreateEncoder(client->connection()->config());

    session_ = new SessionManager(context_->capture_message_loop(),
                                  context_->encode_message_loop(),
                                  context_->main_message_loop(),
                                  capturer_.release(),
                                  encoder);
  }

  // Immediately add the client and start the session.
  session_->AddClient(client);
  session_->Start();
  VLOG(1) << "Session manager started";
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
void ChromotingHost::HandleMessage(ClientConnection* client,
                                   ChromotingClientMessage* message) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  // Delegate the messages to EventExecutor and delete the unhandled
  // messages.
  DCHECK(executor_.get());
  executor_->HandleInputEvent(message);
}

void ChromotingHost::OnConnectionOpened(ClientConnection* client) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  // Completes the client connection.
  VLOG(1) << "Connection to client established.";
  OnClientConnected(client_.get());
}

void ChromotingHost::OnConnectionClosed(ClientConnection* client) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  VLOG(1) << "Connection to client closed.";
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
    VLOG(1) << "Host connected as " << jingle_client->GetFullJid();

    // Create and start |chromotocol_server_|.
    JingleChromotocolServer* server =
        new JingleChromotocolServer(context_->jingle_thread());
    // TODO(ajwong): Make this a command switch when we're more stable.
    server->set_allow_local_ips(true);
    server->Init(jingle_client->GetFullJid(),
                 jingle_client->session_manager(),
                 NewCallback(this, &ChromotingHost::OnNewClientConnection));
    chromotocol_server_ = server;

    // Start heartbeating.
    heartbeat_sender_->Start();
  } else if (state == JingleClient::CLOSED) {
    VLOG(1) << "Host disconnected from talk network.";

    // Stop heartbeating.
    heartbeat_sender_->Stop();

    // TODO(sergeyu): We should try reconnecting here instead of terminating
    // the host.
    Shutdown();
  }
}

void ChromotingHost::OnNewClientConnection(
    ChromotocolConnection* connection,
    ChromotocolServer::IncomingConnectionResponse* response) {
  AutoLock auto_lock(lock_);
  // TODO(hclam): Allow multiple clients to connect to the host.
  if (client_.get() || state_ != kStarted) {
    *response = ChromotocolServer::DECLINE;
    return;
  }

  // Check that the user has access to the host.
  if (!access_verifier_.VerifyPermissions(connection->jid())) {
    *response = ChromotocolServer::DECLINE;
    return;
  }

  scoped_ptr<CandidateChromotocolConfig> local_config(
      CandidateChromotocolConfig::CreateDefault());
  local_config->SetInitialResolution(
      ScreenResolution(capturer_->width(), capturer_->height()));
  // TODO(sergeyu): Respect resolution requested by the client if supported.
  ChromotocolConfig* config =
      local_config->Select(connection->candidate_config(),
                           true /* force_host_resolution */);

  if (!config) {
    LOG(WARNING) << "Rejecting connection from " << connection->jid()
                 << " because no compartible configuration has been found.";
    *response = ChromotocolServer::INCOMPATIBLE;
    return;
  }

  connection->set_config(config);

  *response = ChromotocolServer::ACCEPT;

  VLOG(1) << "Client connected: " << connection->jid();

  // If we accept the connected then create a client object and set the
  // callback.
  client_ = new ClientConnection(context_->main_message_loop(), this);
  client_->Init(connection);
}

void ChromotingHost::OnServerClosed() {
  // Don't need to do anything here.
}

// TODO(sergeyu): Move this to SessionManager?
Encoder* ChromotingHost::CreateEncoder(const ChromotocolConfig* config) {
  const ChannelConfig& video_config = config->video_config();

  if (video_config.codec == ChannelConfig::CODEC_VERBATIM) {
    return new remoting::EncoderVerbatim();
  } else if (video_config.codec == ChannelConfig::CODEC_ZIP) {
    return new remoting::EncoderZlib();
  }
  // TODO(sergeyu): Enable VP8 on ARM builds.
#if !defined(ARCH_CPU_ARM_FAMILY)
  else if (video_config.codec == ChannelConfig::CODEC_VP8) {
    return new remoting::EncoderVp8();
  }
#endif

  return NULL;
}


}  // namespace remoting
