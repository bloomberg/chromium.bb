// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_port_allocator.h"
#include "remoting/host/screen_recorder.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/libjingle_transport_factory.h"
#include "remoting/protocol/session_config.h"

using remoting::protocol::ConnectionToClient;
using remoting::protocol::InputStub;

namespace remoting {

namespace {

const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  2000,

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0,

  // Maximum amount of time we are willing to delay our request in ms.
  -1,

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

}  // namespace

ChromotingHost::ChromotingHost(
    ChromotingHostContext* context,
    SignalStrategy* signal_strategy,
    DesktopEnvironment* environment,
    const NetworkSettings& network_settings)
    : context_(context),
      desktop_environment_(environment),
      network_settings_(network_settings),
      signal_strategy_(signal_strategy),
      stopping_recorders_(0),
      state_(kInitial),
      protocol_config_(protocol::CandidateSessionConfig::CreateDefault()),
      login_backoff_(&kDefaultBackoffPolicy),
      authenticating_client_(false),
      reject_authenticating_client_(false) {
  DCHECK(context_);
  DCHECK(signal_strategy);
  DCHECK(desktop_environment_);
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  desktop_environment_->set_host(this);
}

ChromotingHost::~ChromotingHost() {
  DCHECK(clients_.empty());
}

void ChromotingHost::Start() {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  LOG(INFO) << "Starting host";

  // Make sure this object is not started.
  if (state_ != kInitial)
    return;
  state_ = kStarted;

  // Create port allocator and transport factory.
  scoped_ptr<HostPortAllocator> port_allocator(
      HostPortAllocator::Create(context_->url_request_context_getter(),
                                network_settings_));

  bool incoming_only = network_settings_.nat_traversal_mode ==
      NetworkSettings::NAT_TRAVERSAL_DISABLED;

  scoped_ptr<protocol::TransportFactory> transport_factory(
      new protocol::LibjingleTransportFactory(
          port_allocator.PassAs<cricket::HttpPortAllocatorBase>(),
          incoming_only));

  // Create and start session manager.
  bool fetch_stun_relay_info = network_settings_.nat_traversal_mode ==
      NetworkSettings::NAT_TRAVERSAL_ENABLED;
  session_manager_.reset(new protocol::JingleSessionManager(
      transport_factory.Pass(), fetch_stun_relay_info));
  session_manager_->Init(signal_strategy_, this);
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

  // Destroy session manager.
  session_manager_.reset();

  // Stop screen recorder
  if (recorder_.get()) {
    StopScreenRecorder();
  } else if (!stopping_recorders_) {
    ShutdownFinish();
  }
}

void ChromotingHost::AddStatusObserver(HostStatusObserver* observer) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  status_observers_.AddObserver(observer);
}

void ChromotingHost::RemoveStatusObserver(HostStatusObserver* observer) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  status_observers_.RemoveObserver(observer);
}

void ChromotingHost::RejectAuthenticatingClient() {
  DCHECK(authenticating_client_);
  reject_authenticating_client_ = true;
}

void ChromotingHost::SetAuthenticatorFactory(
    scoped_ptr<protocol::AuthenticatorFactory> authenticator_factory) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  session_manager_->set_authenticator_factory(authenticator_factory.Pass());
}

////////////////////////////////////////////////////////////////////////////
// protocol::ClientSession::EventHandler implementation.
void ChromotingHost::OnSessionAuthenticated(ClientSession* client) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  login_backoff_.Reset();
}

void ChromotingHost::OnSessionChannelsConnected(ClientSession* client) {
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

  // Disconnects above must have destroyed all other clients and |recorder_|.
  DCHECK_EQ(clients_.size(), 1U);
  DCHECK(!recorder_.get());

  // Then we create a ScreenRecorder passing the message loops that
  // it should run on.
  Encoder* encoder = CreateEncoder(client->connection()->session()->config());

  recorder_ = new ScreenRecorder(context_->main_message_loop(),
                                 context_->encode_message_loop(),
                                 context_->network_message_loop(),
                                 desktop_environment_->capturer(),
                                 encoder);

  // Immediately add the connection and start the session.
  recorder_->AddConnection(client->connection());
  recorder_->Start();
  desktop_environment_->OnSessionStarted();

  // Notify observers that there is at least one authenticated client.
  const std::string& jid = client->client_jid();

  reject_authenticating_client_ = false;

  authenticating_client_ = true;
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientAuthenticated(jid));
  authenticating_client_ = false;

  if (reject_authenticating_client_) {
    client->Disconnect();
  }
}

void ChromotingHost::OnSessionAuthenticationFailed(ClientSession* client) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());

  // Notify observers.
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnAccessDenied(client->client_jid()));
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

  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientDisconnected(client->client_jid()));

  if (recorder_.get()) {
    // Currently we don't allow more than one simultaneous connection,
    // so we need to shutdown recorder when a client disconnects.
    StopScreenRecorder();
  }
  desktop_environment_->OnSessionFinished();
}

void ChromotingHost::OnSessionSequenceNumber(ClientSession* session,
                                             int64 sequence_number) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  if (recorder_.get())
    recorder_->UpdateSequenceNumber(sequence_number);
}

void ChromotingHost::OnSessionRouteChange(
    ClientSession* session,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientRouteChange(session->client_jid(), channel_name,
                                        route));
}

void ChromotingHost::OnSessionManagerReady() {
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

  if (login_backoff_.ShouldRejectRequest()) {
    *response = protocol::SessionManager::OVERLOAD;
    return;
  }

  // We treat each incoming connection as a failure to authenticate,
  // and clear the backoff when a connection successfully
  // authenticates. This allows the backoff to protect from parallel
  // connection attempts as well as sequential ones.
  login_backoff_.InformOfRequest(false);

  protocol::SessionConfig config;
  if (!protocol_config_->Select(session->candidate_config(), &config)) {
    LOG(WARNING) << "Rejecting connection from " << session->jid()
                 << " because no compatible configuration has been found.";
    *response = protocol::SessionManager::INCOMPATIBLE;
    return;
  }

  session->set_config(config);

  *response = protocol::SessionManager::ACCEPT;

  LOG(INFO) << "Client connected: " << session->jid();

  // Create a client object.
  scoped_ptr<protocol::ConnectionToClient> connection(
      new protocol::ConnectionToClient(session));
  ClientSession* client = new ClientSession(
      this, connection.Pass(), desktop_environment_->event_executor(),
      desktop_environment_->capturer());
  clients_.push_back(client);
}

void ChromotingHost::set_protocol_config(
    protocol::CandidateSessionConfig* config) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
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
    (*client)->SetDisableInputs(pause);
  }
}

void ChromotingHost::SetUiStrings(const UiStrings& ui_strings) {
  DCHECK(context_->network_message_loop()->BelongsToCurrentThread());
  DCHECK_EQ(state_, kInitial);

  ui_strings_ = ui_strings;
}

// TODO(sergeyu): Move this to SessionManager?
// static
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
  DCHECK(!stopping_recorders_);

  state_ = kStopped;

  // Keep reference to |this|, so that we don't get destroyed while
  // sending notifications.
  scoped_refptr<ChromotingHost> self(this);

  // Notify observers.
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnShutdown());

  for (std::vector<base::Closure>::iterator it = shutdown_tasks_.begin();
       it != shutdown_tasks_.end(); ++it) {
    it->Run();
  }
  shutdown_tasks_.clear();
}

}  // namespace remoting
