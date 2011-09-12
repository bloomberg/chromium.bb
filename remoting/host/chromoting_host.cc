// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/base/encoder.h"
#include "remoting/base/encoder_row_based.h"
#include "remoting/base/encoder_vp8.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/curtain.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/screen_recorder.h"
#include "remoting/host/user_authenticator.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/proto/auth.pb.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/session_config.h"

using remoting::protocol::ConnectionToClient;
using remoting::protocol::InputStub;

namespace remoting {

// static
ChromotingHost* ChromotingHost::Create(ChromotingHostContext* context,
                                       MutableHostConfig* config,
                                       DesktopEnvironment* environment,
                                       AccessVerifier* access_verifier,
                                       bool allow_nat_traversal) {
  return new ChromotingHost(context, config, environment, access_verifier,
                            allow_nat_traversal);
}

ChromotingHost::ChromotingHost(ChromotingHostContext* context,
                               MutableHostConfig* config,
                               DesktopEnvironment* environment,
                               AccessVerifier* access_verifier,
                               bool allow_nat_traversal)
    : context_(context),
      desktop_environment_(environment),
      config_(config),
      access_verifier_(access_verifier),
      allow_nat_traversal_(allow_nat_traversal),
      state_(kInitial),
      stopping_recorders_(0),
      protocol_config_(protocol::CandidateSessionConfig::CreateDefault()),
      is_curtained_(false),
      is_it2me_(false) {
  DCHECK(desktop_environment_);
  desktop_environment_->set_host(this);
}

ChromotingHost::~ChromotingHost() {
}

void ChromotingHost::Start() {
  if (!context_->network_message_loop()->BelongsToCurrentThread()) {
    context_->network_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::Start, this));
    return;
  }

  LOG(INFO) << "Starting host";
  DCHECK(!signal_strategy_.get());
  DCHECK(access_verifier_.get());

  // Make sure this object is not started.
  {
    base::AutoLock auto_lock(lock_);
    if (state_ != kInitial)
      return;
    state_ = kStarted;
  }

  // Use an XMPP connection to the Talk network for session signalling.
  std::string xmpp_login;
  std::string xmpp_auth_token;
  std::string xmpp_auth_service;
  if (!config_->GetString(kXmppLoginConfigPath, &xmpp_login) ||
      !config_->GetString(kXmppAuthTokenConfigPath, &xmpp_auth_token) ||
      !config_->GetString(kXmppAuthServiceConfigPath, &xmpp_auth_service)) {
    LOG(ERROR) << "XMPP credentials are not defined in the config.";
    return;
  }

  signal_strategy_.reset(
      new XmppSignalStrategy(context_->jingle_thread(), xmpp_login,
                             xmpp_auth_token,
                             xmpp_auth_service));
  signal_strategy_->Init(this);
}

// This method is called when we need to destroy the host process.
void ChromotingHost::Shutdown(Task* shutdown_task) {
  if (MessageLoop::current() != context_->main_message_loop()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingHost::Shutdown, this, shutdown_task));
    return;
  }

  // No-op if this object is not started yet.
  {
    base::AutoLock auto_lock(lock_);
    if (state_ == kInitial || state_ == kStopped) {
      // Nothing to do if we are not started.
      state_ = kStopped;
      context_->main_message_loop()->PostTask(FROM_HERE, shutdown_task);
      return;
    }
    if (shutdown_task)
      shutdown_tasks_.push_back(shutdown_task);
    if (state_ == kStopping)
      return;
    state_ = kStopping;
  }

  // Disconnect all of the clients, implicitly stopping the ScreenRecorder.
  while (!clients_.empty()) {
    OnClientDisconnected(clients_.front()->connection());
  }

  ShutdownNetwork();
}

void ChromotingHost::AddStatusObserver(HostStatusObserver* observer) {
  DCHECK_EQ(state_, kInitial);
  status_observers_.push_back(observer);
}

////////////////////////////////////////////////////////////////////////////
// protocol::ConnectionToClient::EventHandler implementations
void ChromotingHost::OnConnectionOpened(ConnectionToClient* connection) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  VLOG(1) << "Connection to client established.";
  // TODO(wez): ChromotingHost shouldn't need to know about Me2Mom.
  if (is_it2me_) {
    context_->main_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::ProcessPreAuthentication, this,
                              make_scoped_refptr(connection)));
  }
}

void ChromotingHost::OnConnectionClosed(ConnectionToClient* connection) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  VLOG(1) << "Connection to client closed.";
  context_->main_message_loop()->PostTask(
      FROM_HERE, base::Bind(&ChromotingHost::OnClientDisconnected, this,
                            make_scoped_refptr(connection)));
}

void ChromotingHost::OnConnectionFailed(ConnectionToClient* connection) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  LOG(ERROR) << "Connection failed unexpectedly.";
  context_->main_message_loop()->PostTask(
      FROM_HERE, base::Bind(&ChromotingHost::OnClientDisconnected, this,
                            make_scoped_refptr(connection)));
}

void ChromotingHost::OnSequenceNumberUpdated(ConnectionToClient* connection,
                                             int64 sequence_number) {
  // Update the sequence number in ScreenRecorder.
  if (MessageLoop::current() != context_->main_message_loop()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::OnSequenceNumberUpdated, this,
                              make_scoped_refptr(connection), sequence_number));
    return;
  }

  if (recorder_.get())
    recorder_->UpdateSequenceNumber(sequence_number);
}

////////////////////////////////////////////////////////////////////////////
// SignalStrategy::StatusObserver implementations
void ChromotingHost::OnStateChange(
    SignalStrategy::StatusObserver::State state) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  if (state == SignalStrategy::StatusObserver::CONNECTED) {
    LOG(INFO) << "Host connected as " << local_jid_;

    // Create and start session manager.
    protocol::JingleSessionManager* server =
        protocol::JingleSessionManager::CreateNotSandboxed(
            context_->network_message_loop());
    // TODO(ajwong): Make this a command switch when we're more stable.
    server->set_allow_local_ips(true);

    // Assign key and certificate to server.
    HostKeyPair key_pair;
    CHECK(key_pair.Load(config_))
        << "Failed to load server authentication data";

    server->Init(local_jid_, signal_strategy_.get(), this,
                 key_pair.CopyPrivateKey(), key_pair.GenerateCertificate(),
                 allow_nat_traversal_);

    session_manager_.reset(server);

    for (StatusObserverList::iterator it = status_observers_.begin();
         it != status_observers_.end(); ++it) {
      (*it)->OnSignallingConnected(signal_strategy_.get(), local_jid_);
    }
  } else if (state == SignalStrategy::StatusObserver::CLOSED) {
    LOG(INFO) << "Host disconnected from talk network.";
    for (StatusObserverList::iterator it = status_observers_.begin();
         it != status_observers_.end(); ++it) {
      (*it)->OnSignallingDisconnected();
    }
  }
}

void ChromotingHost::OnJidChange(const std::string& full_jid) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  local_jid_ = full_jid;
}

void ChromotingHost::OnSessionManagerInitialized() {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  // Don't need to do anything here, just wait for incoming
  // connections.
}

void ChromotingHost::OnIncomingSession(
      protocol::Session* session,
      protocol::SessionManager::IncomingSessionResponse* response) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  if (state_ != kStarted) {
    *response = protocol::SessionManager::DECLINE;
    return;
  }

  // Check that the client has access to the host.
  if (!access_verifier_->VerifyPermissions(session->jid(),
                                           session->initiator_token())) {
    *response = protocol::SessionManager::DECLINE;

    // Notify observers.
    for (StatusObserverList::iterator it = status_observers_.begin();
         it != status_observers_.end(); ++it) {
      (*it)->OnAccessDenied();
    }
    return;
  }

  // If we are running Me2Mom and already have an authenticated client then
  // one of the connections may be an attacker, so both are suspect.
  if (is_it2me_ && AuthenticatedClientsCount() > 0) {
    *response = protocol::SessionManager::DECLINE;

    // Close existing sessions and shutdown the host.
    Shutdown(NULL);
    return;
  }

  // TODO(simonmorris): The resolution is set in the video stream now,
  // so it doesn't need to be set here.
  *protocol_config_->mutable_initial_resolution() =
      protocol::ScreenResolution(2048, 2048);
  // TODO(sergeyu): Respect resolution requested by the client if supported.
  protocol::SessionConfig config;
  if (!protocol_config_->Select(session->candidate_config(),
                                true /* force_host_resolution */, &config)) {
    LOG(WARNING) << "Rejecting connection from " << session->jid()
                 << " because no compatible configuration has been found.";
    *response = protocol::SessionManager::INCOMPATIBLE;
    return;
  }

  session->set_config(config);
  session->set_receiver_token(
      GenerateHostAuthToken(session->initiator_token()));

  // Provide the Access Code as shared secret for SSL channel authentication.
  session->set_shared_secret(access_code_);

  *response = protocol::SessionManager::ACCEPT;

  LOG(INFO) << "Client connected: " << session->jid();

  // We accept the connection, so create a connection object.
  ConnectionToClient* connection = new ConnectionToClient(
      context_->network_message_loop(), this);

  // Create a client object.
  ClientSession* client = new ClientSession(
      this,
      UserAuthenticator::Create(),
      connection,
      desktop_environment_->event_executor());
  connection->set_host_stub(client);
  connection->set_input_stub(client);

  connection->Init(session);

  clients_.push_back(client);
}

void ChromotingHost::set_protocol_config(
    protocol::CandidateSessionConfig* config) {
  DCHECK(config_.get());
  DCHECK_EQ(state_, kInitial);
  protocol_config_.reset(config);
}

void ChromotingHost::LocalMouseMoved(const gfx::Point& new_pos) {
  if (!context_->network_message_loop()->BelongsToCurrentThread()) {
    context_->network_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::LocalMouseMoved, this, new_pos));
    return;
  }
  ClientList::iterator client;
  for (client = clients_.begin(); client != clients_.end(); ++client) {
    client->get()->LocalMouseMoved(new_pos);
  }
}

void ChromotingHost::PauseSession(bool pause) {
  if (context_->main_message_loop() != MessageLoop::current()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &ChromotingHost::PauseSession,
                          pause));
    return;
  }
  ClientList::iterator client;
  for (client = clients_.begin(); client != clients_.end(); ++client) {
    client->get()->set_awaiting_continue_approval(pause);
  }
  desktop_environment_->OnPause(pause);
}

void ChromotingHost::SetUiStrings(const UiStrings& ui_strings) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());
  DCHECK_EQ(state_, kInitial);

  ui_strings_ = ui_strings;
}

void ChromotingHost::OnClientDisconnected(ConnectionToClient* connection) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());

  // Find the client session corresponding to the given connection.
  ClientList::iterator client;
  for (client = clients_.begin(); client != clients_.end(); ++client) {
    if (client->get()->connection() == connection)
      break;
  }
  if (client == clients_.end())
    return;

  // Remove the connection from the session manager and stop the session.
  // TODO(hclam): Stop only if the last connection disconnected.
  if (recorder_.get()) {
    recorder_->RemoveConnection(connection);
    // The recorder only exists to serve the unique authenticated client.
    // If that client has disconnected, then we can kill the recorder.
    if (client->get()->authenticated())
      StopScreenRecorder();
  }

  // Close the connection to client just to be safe.
  connection->Disconnect();

  // Also remove reference to ConnectionToClient from this object.
  int old_authenticated_clients = AuthenticatedClientsCount();
  clients_.erase(client);

  // Notify the observers of the change, if any.
  int authenticated_clients = AuthenticatedClientsCount();
  if (old_authenticated_clients != authenticated_clients) {
    for (StatusObserverList::iterator it = status_observers_.begin();
         it != status_observers_.end(); ++it) {
      (*it)->OnClientDisconnected(connection);
    }
  }

  // Disable the "curtain" if there are no more active clients.
  if (AuthenticatedClientsCount() == 0) {
    EnableCurtainMode(false);
    if (is_it2me_) {
      desktop_environment_->OnLastDisconnect();
    }
  }
}

// TODO(sergeyu): Move this to SessionManager?
Encoder* ChromotingHost::CreateEncoder(const protocol::SessionConfig& config) {
  const protocol::ChannelConfig& video_config = config.video_config();

  if (video_config.codec == protocol::ChannelConfig::CODEC_VERBATIM) {
    return EncoderRowBased::CreateVerbatimEncoder();
  } else if (video_config.codec == protocol::ChannelConfig::CODEC_ZIP) {
    return EncoderRowBased::CreateZlibEncoder();
  } else if (video_config.codec == protocol::ChannelConfig::CODEC_VP8) {
    return new remoting::EncoderVp8();
  }

  return NULL;
}

std::string ChromotingHost::GenerateHostAuthToken(
    const std::string& encoded_client_token) {
  // TODO(ajwong): Return the signature of this instead.
  return encoded_client_token;
}

int ChromotingHost::AuthenticatedClientsCount() const {
  int authenticated_clients = 0;
  for (ClientList::const_iterator it = clients_.begin(); it != clients_.end();
       ++it) {
    if (it->get()->authenticated())
      ++authenticated_clients;
  }
  return authenticated_clients;
}

void ChromotingHost::EnableCurtainMode(bool enable) {
  // TODO(jamiewalch): This will need to be more sophisticated when we think
  // about proper crash recovery and daemon mode.
  // TODO(wez): CurtainMode shouldn't be driven directly by ChromotingHost.
  if (is_it2me_ || enable == is_curtained_)
    return;
  desktop_environment_->curtain()->EnableCurtainMode(enable);
  is_curtained_ = enable;
}

void ChromotingHost::LocalLoginSucceeded(
    scoped_refptr<ConnectionToClient> connection) {
  if (MessageLoop::current() != context_->main_message_loop()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::LocalLoginSucceeded, this,
                              connection));
    return;
  }

  protocol::LocalLoginStatus* status = new protocol::LocalLoginStatus();
  status->set_success(true);
  connection->client_stub()->BeginSessionResponse(
      status, new DeleteTask<protocol::LocalLoginStatus>(status));

  // Disconnect all other clients.
  // Iterate over a copy of the list of clients, to avoid mutating the list
  // while iterating over it.
  ClientList clients_copy(clients_);
  for (ClientList::const_iterator client = clients_copy.begin();
       client != clients_copy.end(); client++) {
    ConnectionToClient* connection_other = client->get()->connection();
    if (connection_other != connection) {
      OnClientDisconnected(connection_other);
    }
  }
  // Those disconnections should have killed the screen recorder.
  CHECK(recorder_.get() == NULL);

  // Create a new RecordSession if there was none.
  if (!recorder_.get()) {
    // Then we create a ScreenRecorder passing the message loops that
    // it should run on.
    Encoder* encoder = CreateEncoder(connection->session()->config());

    recorder_ = new ScreenRecorder(context_->main_message_loop(),
                                   context_->encode_message_loop(),
                                   context_->network_message_loop(),
                                   desktop_environment_->capturer(),
                                   encoder);
  }

  // Immediately add the connection and start the session.
  recorder_->AddConnection(connection);
  recorder_->Start();
  // Notify observers that there is at least one authenticated client.
  for (StatusObserverList::iterator it = status_observers_.begin();
       it != status_observers_.end(); ++it) {
    (*it)->OnClientAuthenticated(connection);
  }
  // TODO(jamiewalch): Tidy up actions to be taken on connect/disconnect,
  // including closing the connection on failure of a critical operation.
  EnableCurtainMode(true);
  if (is_it2me_) {
    std::string username = connection->session()->jid();
    size_t pos = username.find('/');
    if (pos != std::string::npos)
      username.replace(pos, std::string::npos, "");
    desktop_environment_->OnConnect(username);
  }
}

void ChromotingHost::LocalLoginFailed(
    scoped_refptr<ConnectionToClient> connection) {
  if (MessageLoop::current() != context_->main_message_loop()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::LocalLoginFailed, this,
                              connection));
    return;
  }

  protocol::LocalLoginStatus* status = new protocol::LocalLoginStatus();
  status->set_success(false);
  connection->client_stub()->BeginSessionResponse(
      status, new DeleteTask<protocol::LocalLoginStatus>(status));
}

void ChromotingHost::ProcessPreAuthentication(
    const scoped_refptr<ConnectionToClient>& connection) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());
  // Find the client session corresponding to the given connection.
  ClientList::iterator client;
  for (client = clients_.begin(); client != clients_.end(); ++client) {
    if (client->get()->connection() == connection)
      break;
  }
  CHECK(client != clients_.end());
  client->get()->OnAuthorizationComplete(true);
}

void ChromotingHost::StopScreenRecorder() {
  DCHECK(MessageLoop::current() == context_->main_message_loop());
  DCHECK(recorder_.get());

  ++stopping_recorders_;
  recorder_->Stop(base::Bind(&ChromotingHost::OnScreenRecorderStopped, this));
  recorder_ = NULL;
}

void ChromotingHost::OnScreenRecorderStopped() {
  if (MessageLoop::current() != context_->main_message_loop()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::OnScreenRecorderStopped, this));
    return;
  }

  --stopping_recorders_;
  DCHECK_GE(stopping_recorders_, 0);

  bool stopping;
  {
    base::AutoLock auto_lock(lock_);
    stopping = state_ == kStopping;
  }

  if (!stopping_recorders_ && stopping)
    ShutdownFinish();
}

void ChromotingHost::ShutdownNetwork() {
  if (!context_->network_message_loop()->BelongsToCurrentThread()) {
    context_->network_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::ShutdownNetwork, this));
    return;
  }

  // Stop chromotocol session manager.
  if (session_manager_.get()) {
    session_manager_->Close();
    session_manager_.reset();
  }

  // Stop XMPP connection.
  if (signal_strategy_.get()) {
    signal_strategy_->Close();
    signal_strategy_.reset();

    for (StatusObserverList::iterator it = status_observers_.begin();
         it != status_observers_.end(); ++it) {
      (*it)->OnSignallingDisconnected();
    }
  }

  ShutdownRecorder();
}

void ChromotingHost::ShutdownRecorder() {
  if (MessageLoop::current() != context_->main_message_loop()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::ShutdownRecorder, this));
    return;
  }

  if (recorder_.get()) {
    StopScreenRecorder();
  } else if (!stopping_recorders_) {
    ShutdownFinish();
  }
}

void ChromotingHost::ShutdownFinish() {
  if (MessageLoop::current() != context_->main_message_loop()) {
    context_->main_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::ShutdownFinish, this));
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    state_ = kStopped;
  }

  // Keep reference to |this|, so that we don't get destroyed while
  // sending notifications.
  scoped_refptr<ChromotingHost> self(this);

  // Notify observers.
  for (StatusObserverList::iterator it = status_observers_.begin();
       it != status_observers_.end(); ++it) {
    (*it)->OnShutdown();
  }

  for (std::vector<Task*>::iterator it = shutdown_tasks_.begin();
       it != shutdown_tasks_.end(); ++it) {
    (*it)->Run();
    delete *it;
  }
  shutdown_tasks_.clear();
}

}  // namespace remoting
