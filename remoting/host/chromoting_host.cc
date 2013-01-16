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
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/host_config.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
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
    SignalStrategy* signal_strategy,
    DesktopEnvironmentFactory* desktop_environment_factory,
    scoped_ptr<protocol::SessionManager> session_manager,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : desktop_environment_factory_(desktop_environment_factory),
      session_manager_(session_manager.Pass()),
      audio_task_runner_(audio_task_runner),
      input_task_runner_(input_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      video_encode_task_runner_(video_encode_task_runner),
      network_task_runner_(network_task_runner),
      ui_task_runner_(ui_task_runner),
      signal_strategy_(signal_strategy),
      state_(kInitial),
      protocol_config_(protocol::CandidateSessionConfig::CreateDefault()),
      login_backoff_(&kDefaultBackoffPolicy),
      authenticating_client_(false),
      reject_authenticating_client_(false) {
  DCHECK(signal_strategy);
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!desktop_environment_factory_->SupportsAudioCapture()) {
    protocol::CandidateSessionConfig::DisableAudioChannel(
        protocol_config_.get());
  }
}

ChromotingHost::~ChromotingHost() {
  DCHECK(clients_.empty());
}

void ChromotingHost::Start(const std::string& xmpp_login) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  LOG(INFO) << "Starting host";

  // Make sure this object is not started.
  if (state_ != kInitial)
    return;
  state_ = kStarted;

  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnStart(xmpp_login));

  // Start the SessionManager, supplying this ChromotingHost as the listener.
  session_manager_->Init(signal_strategy_, this);
}

// This method is called when we need to destroy the host process.
void ChromotingHost::Shutdown(const base::Closure& shutdown_task) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::Shutdown, this, shutdown_task));
    return;
  }

  switch (state_) {
    case kInitial:
    case kStopped:
      // Nothing to do if we are not started.
      state_ = kStopped;
      if (!shutdown_task.is_null())
        network_task_runner_->PostTask(FROM_HERE, shutdown_task);
      break;

    case kStopping:
      // We are already stopping. Just save the task.
      if (!shutdown_task.is_null())
        shutdown_tasks_.push_back(shutdown_task);
      break;

    case kStarted:
      if (!shutdown_task.is_null())
        shutdown_tasks_.push_back(shutdown_task);
      state_ = kStopping;

      // Disconnect all of the clients.
      while (!clients_.empty()) {
        clients_.front()->Disconnect();
      }

      // Run the remaining shutdown tasks.
      if (state_ == kStopping)
        ShutdownFinish();

      break;
  }
}

void ChromotingHost::AddStatusObserver(HostStatusObserver* observer) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  status_observers_.AddObserver(observer);
}

void ChromotingHost::RemoveStatusObserver(HostStatusObserver* observer) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  status_observers_.RemoveObserver(observer);
}

void ChromotingHost::RejectAuthenticatingClient() {
  DCHECK(authenticating_client_);
  reject_authenticating_client_ = true;
}

void ChromotingHost::SetAuthenticatorFactory(
    scoped_ptr<protocol::AuthenticatorFactory> authenticator_factory) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  session_manager_->set_authenticator_factory(authenticator_factory.Pass());
}

void ChromotingHost::SetMaximumSessionDuration(
    const base::TimeDelta& max_session_duration) {
  max_session_duration_ = max_session_duration;
}

////////////////////////////////////////////////////////////////////////////
// protocol::ClientSession::EventHandler implementation.
void ChromotingHost::OnSessionAuthenticated(ClientSession* client) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  login_backoff_.Reset();

  // Disconnect all other clients.
  // Iterate over a copy of the list of clients, to avoid mutating the list
  // while iterating over it.
  ClientList clients_copy(clients_);
  for (ClientList::const_iterator other_client = clients_copy.begin();
       other_client != clients_copy.end(); ++other_client) {
    if (other_client->get() != client) {
      (*other_client)->Disconnect();
    }
  }

  // Disconnects above must have destroyed all other clients and |recorder_|.
  DCHECK_EQ(clients_.size(), 1U);

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

void ChromotingHost::OnSessionChannelsConnected(ClientSession* client) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  // Notify observers.
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientConnected(client->client_jid()));
}

void ChromotingHost::OnSessionAuthenticationFailed(ClientSession* client) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  // Notify observers.
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnAccessDenied(client->client_jid()));
}

void ChromotingHost::OnSessionClosed(ClientSession* client) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  ClientList::iterator it = clients_.begin();
  for (; it != clients_.end(); ++it) {
    if (it->get() == client) {
      break;
    }
  }
  CHECK(it != clients_.end());

  if (client->is_authenticated()) {
    FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                      OnClientDisconnected(client->client_jid()));
  }

  client->Stop();
  clients_.erase(it);

  if (state_ == kStopping && clients_.empty())
    ShutdownFinish();
}

void ChromotingHost::OnSessionSequenceNumber(ClientSession* session,
                                             int64 sequence_number) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
}

void ChromotingHost::OnSessionRouteChange(
    ClientSession* session,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientRouteChange(session->client_jid(), channel_name,
                                        route));
}

void ChromotingHost::OnClientDimensionsChanged(ClientSession* session,
                                               const SkISize& size) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientDimensionsChanged(session->client_jid(), size));
}

void ChromotingHost::OnSessionManagerReady() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  // Don't need to do anything here, just wait for incoming
  // connections.
}

void ChromotingHost::OnIncomingSession(
      protocol::Session* session,
      protocol::SessionManager::IncomingSessionResponse* response) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

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
  scoped_refptr<ClientSession> client = new ClientSession(
      this,
      audio_task_runner_,
      input_task_runner_,
      video_capture_task_runner_,
      video_encode_task_runner_,
      network_task_runner_,
      ui_task_runner_,
      connection.Pass(),
      desktop_environment_factory_,
      max_session_duration_);
  clients_.push_back(client);
}

void ChromotingHost::set_protocol_config(
    scoped_ptr<protocol::CandidateSessionConfig> config) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(config.get());
  DCHECK_EQ(state_, kInitial);
  protocol_config_ = config.Pass();
}

void ChromotingHost::OnLocalMouseMoved(const SkIPoint& new_pos) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::OnLocalMouseMoved,
                              this, new_pos));
    return;
  }

  ClientList::iterator client;
  for (client = clients_.begin(); client != clients_.end(); ++client) {
    (*client)->LocalMouseMoved(new_pos);
  }
}

void ChromotingHost::PauseSession(bool pause) {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::PauseSession, this, pause));
    return;
  }

  ClientList::iterator client;
  for (client = clients_.begin(); client != clients_.end(); ++client) {
    (*client)->SetDisableInputs(pause);
  }
}

void ChromotingHost::DisconnectAllClients() {
  if (!network_task_runner_->BelongsToCurrentThread()) {
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::DisconnectAllClients, this));
    return;
  }

  while (!clients_.empty()) {
    size_t size = clients_.size();
    clients_.front()->Disconnect();
    CHECK_EQ(clients_.size(), size - 1);
  }
}

void ChromotingHost::SetUiStrings(const UiStrings& ui_strings) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kInitial);

  ui_strings_ = ui_strings;
}

void ChromotingHost::ShutdownFinish() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopping);

  state_ = kStopped;

  // Destroy session manager.
  session_manager_.reset();

  // Clear |desktop_environment_factory_| and |signal_strategy_| to
  // ensure we don't try to touch them after running shutdown tasks
  desktop_environment_factory_ = NULL;
  signal_strategy_ = NULL;

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
