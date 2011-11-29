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
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
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
                                       bool allow_nat_traversal) {
  return new ChromotingHost(context, config, environment, allow_nat_traversal);
}

ChromotingHost::ChromotingHost(ChromotingHostContext* context,
                               MutableHostConfig* config,
                               DesktopEnvironment* environment,
                               bool allow_nat_traversal)
    : context_(context),
      desktop_environment_(environment),
      config_(config),
      allow_nat_traversal_(allow_nat_traversal),
      stopping_recorders_(0),
      state_(kInitial),
      protocol_config_(protocol::CandidateSessionConfig::CreateDefault()),
      is_curtained_(false),
      is_it2me_(false) {
  DCHECK(desktop_environment_);
  desktop_environment_->set_host(this);
}

ChromotingHost::~ChromotingHost() {
  DCHECK(clients_.empty());
}

void ChromotingHost::Start() {
  if (!context_->network_message_loop()->BelongsToCurrentThread()) {
    context_->network_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::Start, this));
    return;
  }

  LOG(INFO) << "Starting host";
  DCHECK(!signal_strategy_.get());

  // Make sure this object is not started.
  if (state_ != kInitial)
    return;
  state_ = kStarted;

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
void ChromotingHost::Shutdown(const base::Closure& shutdown_task) {
  if (!context_->network_message_loop()->BelongsToCurrentThread()) {
    context_->network_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::Shutdown, this, shutdown_task));
    return;
  }

  // No-op if this object is not started yet.
  if (state_ == kInitial || state_ == kStopped) {
      // Nothing to do if we are not started.
    state_ = kStopped;
    context_->network_message_loop()->PostTask(FROM_HERE, shutdown_task);
    return;
  }
  if (!shutdown_task.is_null())
    shutdown_tasks_.push_back(shutdown_task);
  if (state_ == kStopping)
    return;
  state_ = kStopping;

  // Disconnect all of the clients, implicitly stopping the ScreenRecorder.
  while (!clients_.empty()) {
    clients_.front()->Disconnect();
  }

  // Stop session manager.
  if (session_manager_.get()) {
    session_manager_->Close();
    // It may not be safe to delete |session_manager_| here becase
    // this method may be invoked in response to a libjingle event and
    // libjingle's sigslot doesn't handle it properly, so postpone the
    // deletion.
    context_->network_message_loop()->DeleteSoon(
        FROM_HERE, session_manager_.release());
  }

  // Stop XMPP connection synchronously.
  if (signal_strategy_.get()) {
    signal_strategy_->Close();
    signal_strategy_.reset();

    for (StatusObserverList::iterator it = status_observers_.begin();
         it != status_observers_.end(); ++it) {
      (*it)->OnSignallingDisconnected();
    }
  }

  if (recorder_.get()) {
    StopScreenRecorder();
  } else {
    ShutdownFinish();
  }
}

void ChromotingHost::AddStatusObserver(HostStatusObserver* observer) {
  DCHECK_EQ(state_, kInitial);
  status_observers_.push_back(observer);
}

////////////////////////////////////////////////////////////////////////////
// protocol::ClientSession::EventHandler implementation.
void ChromotingHost::OnSessionAuthenticated(ClientSession* client) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  // Disconnect all other clients.
  // Iterate over a copy of the list of clients, to avoid mutating the list
  // while iterating over it.
  ClientList clients_copy(clients_);
  for (ClientList::const_iterator other_client = clients_copy.begin();
       other_client != clients_copy.end(); ++other_client) {
    if ((*other_client) != client) {
      (*other_client)->Disconnect();
    }
  }

  // Create a new RecordSession if there was none.
  if (!recorder_.get()) {
    // Then we create a ScreenRecorder passing the message loops that
    // it should run on.
    Encoder* encoder = CreateEncoder(client->connection()->session()->config());

    recorder_ = new ScreenRecorder(context_->main_message_loop(),
                                   context_->encode_message_loop(),
                                   context_->network_message_loop(),
                                   desktop_environment_->capturer(),
                                   encoder);
  }

  // Immediately add the connection and start the session.
  recorder_->AddConnection(client->connection());
  recorder_->Start();

  // Notify observers that there is at least one authenticated client.
  const std::string& jid = client->connection()->session()->jid();
  for (StatusObserverList::iterator it = status_observers_.begin();
       it != status_observers_.end(); ++it) {
    (*it)->OnClientAuthenticated(jid);
  }
  // TODO(jamiewalch): Tidy up actions to be taken on connect/disconnect,
  // including closing the connection on failure of a critical operation.
  EnableCurtainMode(true);

  std::string username = jid.substr(0, jid.find('/'));
  desktop_environment_->OnConnect(username);
}

void ChromotingHost::OnSessionAuthenticationFailed(ClientSession* client) {
  // Notify observers.
  for (StatusObserverList::iterator it = status_observers_.begin();
       it != status_observers_.end(); ++it) {
    (*it)->OnAccessDenied();
  }
}

void ChromotingHost::OnSessionClosed(ClientSession* client) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  scoped_ptr<ClientSession> client_destroyer(client);

  ClientList::iterator it = std::find(clients_.begin(), clients_.end(), client);
  CHECK(it != clients_.end());
  clients_.erase(it);

  if (recorder_.get()) {
    recorder_->RemoveConnection(client->connection());
  }

  for (StatusObserverList::iterator it = status_observers_.begin();
       it != status_observers_.end(); ++it) {
    (*it)->OnClientDisconnected(client->client_jid());
  }

  if (AuthenticatedClientsCount() == 0) {
    if (recorder_.get()) {
      // Stop the recorder if there are no more clients.
      StopScreenRecorder();
    }

    // Disable the "curtain" if there are no more active clients.
    EnableCurtainMode(false);
    desktop_environment_->OnLastDisconnect();
  }
}

void ChromotingHost::OnSessionSequenceNumber(ClientSession* session,
                                             int64 sequence_number) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
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
        new protocol::JingleSessionManager(context_->network_message_loop());

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

  if (state_ != kStarted) {
    *response = protocol::SessionManager::DECLINE;
    return;
  }

  // If we are running Me2Mom and already have an authenticated client then
  // one of the connections may be an attacker, so both are suspect.
  if (is_it2me_ && AuthenticatedClientsCount() > 0) {
    *response = protocol::SessionManager::DECLINE;

    // Close existing sessions and shutdown the host.
    Shutdown(base::Closure());
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
  // Provide the Access Code as shared secret for SSL channel authentication.
  session->set_shared_secret(access_code_);

  *response = protocol::SessionManager::ACCEPT;

  LOG(INFO) << "Client connected: " << session->jid();

  // Create a client object.
  protocol::ConnectionToClient* connection =
      new protocol::ConnectionToClient(session);
  ClientSession* client = new ClientSession(
      this, connection, desktop_environment_->event_executor(),
      desktop_environment_->capturer());
  clients_.push_back(client);
}

void ChromotingHost::set_protocol_config(
    protocol::CandidateSessionConfig* config) {
  DCHECK(config);
  DCHECK_EQ(state_, kInitial);
  protocol_config_.reset(config);
}

void ChromotingHost::LocalMouseMoved(const SkIPoint& new_pos) {
  if (!context_->network_message_loop()->BelongsToCurrentThread()) {
    context_->network_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::LocalMouseMoved, this, new_pos));
    return;
  }

  ClientList::iterator client;
  for (client = clients_.begin(); client != clients_.end(); ++client) {
    (*client)->LocalMouseMoved(new_pos);
  }
}

void ChromotingHost::PauseSession(bool pause) {
  if (!context_->network_message_loop()->BelongsToCurrentThread()) {
    context_->network_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::PauseSession, this, pause));
    return;
  }

  ClientList::iterator client;
  for (client = clients_.begin(); client != clients_.end(); ++client) {
    (*client)->set_awaiting_continue_approval(pause);
  }
  desktop_environment_->OnPause(pause);
}

void ChromotingHost::SetUiStrings(const UiStrings& ui_strings) {
  DCHECK_EQ(context_->main_message_loop(), MessageLoop::current());
  DCHECK_EQ(state_, kInitial);

  ui_strings_ = ui_strings;
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

int ChromotingHost::AuthenticatedClientsCount() const {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  int authenticated_clients = 0;
  for (ClientList::const_iterator it = clients_.begin(); it != clients_.end();
       ++it) {
    if ((*it)->authenticated())
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

void ChromotingHost::StopScreenRecorder() {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  DCHECK(recorder_.get());

  ++stopping_recorders_;
  scoped_refptr<ScreenRecorder> recorder = recorder_;
  recorder_ = NULL;
  recorder->Stop(base::Bind(&ChromotingHost::OnScreenRecorderStopped, this));
}

void ChromotingHost::OnScreenRecorderStopped() {
  if (!context_->network_message_loop()->BelongsToCurrentThread()) {
    context_->network_message_loop()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::OnScreenRecorderStopped, this));
    return;
  }

  --stopping_recorders_;
  DCHECK_GE(stopping_recorders_, 0);

  if (!stopping_recorders_ && state_ == kStopping)
    ShutdownFinish();
}

void ChromotingHost::ShutdownFinish() {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  state_ = kStopped;

  // Keep reference to |this|, so that we don't get destroyed while
  // sending notifications.
  scoped_refptr<ChromotingHost> self(this);

  // Notify observers.
  for (StatusObserverList::iterator it = status_observers_.begin();
       it != status_observers_.end(); ++it) {
    (*it)->OnShutdown();
  }

  for (std::vector<base::Closure>::iterator it = shutdown_tasks_.begin();
       it != shutdown_tasks_.end(); ++it) {
    it->Run();
  }
  shutdown_tasks_.clear();
}

}  // namespace remoting
