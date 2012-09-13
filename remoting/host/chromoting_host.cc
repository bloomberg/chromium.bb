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
#include "remoting/host/desktop_environment_factory.h"
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
    ChromotingHostContext* context,
    SignalStrategy* signal_strategy,
    DesktopEnvironmentFactory* desktop_environment_factory,
    scoped_ptr<protocol::SessionManager> session_manager)
    : context_(context),
      desktop_environment_factory_(desktop_environment_factory),
      session_manager_(session_manager.Pass()),
      signal_strategy_(signal_strategy),
      clients_count_(0),
      state_(kInitial),
      protocol_config_(protocol::CandidateSessionConfig::CreateDefault()),
      login_backoff_(&kDefaultBackoffPolicy),
      authenticating_client_(false),
      reject_authenticating_client_(false) {
  DCHECK(context_);
  DCHECK(signal_strategy);
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  if (!desktop_environment_factory_->SupportsAudioCapture()) {
    protocol::CandidateSessionConfig::DisableAudioChannel(
        protocol_config_.get());
  }
}

ChromotingHost::~ChromotingHost() {
  DCHECK(clients_.empty());
}

void ChromotingHost::Start(const std::string& xmpp_login) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

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
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::Shutdown, this, shutdown_task));
    return;
  }

  switch (state_) {
    case kInitial:
    case kStopped:
      // Nothing to do if we are not started.
      state_ = kStopped;
      if (!shutdown_task.is_null())
        context_->network_task_runner()->PostTask(FROM_HERE, shutdown_task);
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
      if (state_ == kStopping && !clients_count_)
        ShutdownFinish();

      break;
  }
}

void ChromotingHost::AddStatusObserver(HostStatusObserver* observer) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  status_observers_.AddObserver(observer);
}

void ChromotingHost::RemoveStatusObserver(HostStatusObserver* observer) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  status_observers_.RemoveObserver(observer);
}

void ChromotingHost::RejectAuthenticatingClient() {
  DCHECK(authenticating_client_);
  reject_authenticating_client_ = true;
}

void ChromotingHost::SetAuthenticatorFactory(
    scoped_ptr<protocol::AuthenticatorFactory> authenticator_factory) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  session_manager_->set_authenticator_factory(authenticator_factory.Pass());
}

void ChromotingHost::SetMaximumSessionDuration(
    const base::TimeDelta& max_session_duration) {
  max_session_duration_ = max_session_duration;
}

////////////////////////////////////////////////////////////////////////////
// protocol::ClientSession::EventHandler implementation.
void ChromotingHost::OnSessionAuthenticated(ClientSession* client) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

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
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  // Notify observers.
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientConnected(client->client_jid()));
}

void ChromotingHost::OnSessionAuthenticationFailed(ClientSession* client) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  // Notify observers.
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnAccessDenied(client->client_jid()));
}

void ChromotingHost::OnSessionClosed(ClientSession* client) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

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

  client->Stop(base::Bind(&ChromotingHost::OnClientStopped, this));
  clients_.erase(it);
}

void ChromotingHost::OnSessionSequenceNumber(ClientSession* session,
                                             int64 sequence_number) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
}

void ChromotingHost::OnSessionRouteChange(
    ClientSession* session,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientRouteChange(session->client_jid(), channel_name,
                                        route));
}

void ChromotingHost::OnSessionManagerReady() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  // Don't need to do anything here, just wait for incoming
  // connections.
}

void ChromotingHost::OnIncomingSession(
      protocol::Session* session,
      protocol::SessionManager::IncomingSessionResponse* response) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

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

  // Create the desktop integration implementation for the client to use.
  scoped_ptr<DesktopEnvironment> desktop_environment =
      desktop_environment_factory_->Create(context_);

  // Create a client object.
  scoped_ptr<protocol::ConnectionToClient> connection(
      new protocol::ConnectionToClient(session));
  scoped_refptr<ClientSession> client = new ClientSession(
      this,
      context_->capture_task_runner(),
      context_->encode_task_runner(),
      context_->network_task_runner(),
      connection.Pass(),
      desktop_environment.Pass(),
      max_session_duration_);
  clients_.push_back(client);
  clients_count_++;
}

void ChromotingHost::set_protocol_config(
    scoped_ptr<protocol::CandidateSessionConfig> config) {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(config.get());
  DCHECK_EQ(state_, kInitial);
  protocol_config_ = config.Pass();
}

void ChromotingHost::OnLocalMouseMoved(const SkIPoint& new_pos) {
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
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
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingHost::PauseSession, this, pause));
    return;
  }

  ClientList::iterator client;
  for (client = clients_.begin(); client != clients_.end(); ++client) {
    (*client)->SetDisableInputs(pause);
  }
}

void ChromotingHost::DisconnectAllClients() {
  if (!context_->network_task_runner()->BelongsToCurrentThread()) {
    context_->network_task_runner()->PostTask(
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
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(state_, kInitial);

  ui_strings_ = ui_strings;
}

void ChromotingHost::OnClientStopped() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());

  --clients_count_;
  if (state_ == kStopping && !clients_count_)
    ShutdownFinish();
}

void ChromotingHost::ShutdownFinish() {
  DCHECK(context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(state_, kStopping);

  // Destroy session manager.
  session_manager_.reset();

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
