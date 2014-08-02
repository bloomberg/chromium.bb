// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop_proxy.h"
#include "build/build_config.h"
#include "jingle/glue/thread_wrapper.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_config.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/video_frame_recorder.h"
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
      started_(false),
      protocol_config_(protocol::CandidateSessionConfig::CreateDefault()),
      login_backoff_(&kDefaultBackoffPolicy),
      authenticating_client_(false),
      reject_authenticating_client_(false),
      enable_curtaining_(false),
      weak_factory_(this) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(signal_strategy);

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

  if (!desktop_environment_factory_->SupportsAudioCapture()) {
    protocol_config_->DisableAudioChannel();
  }
}

ChromotingHost::~ChromotingHost() {
  DCHECK(CalledOnValidThread());

  // Disconnect all of the clients.
  while (!clients_.empty()) {
    clients_.front()->DisconnectSession();
  }

  // Destroy the session manager to make sure that |signal_strategy_| does not
  // have any listeners registered.
  session_manager_.reset();

  // Notify observers.
  if (started_)
    FOR_EACH_OBSERVER(HostStatusObserver, status_observers_, OnShutdown());
}

void ChromotingHost::Start(const std::string& host_owner) {
  DCHECK(CalledOnValidThread());
  DCHECK(!started_);

  HOST_LOG << "Starting host";
  started_ = true;
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_, OnStart(host_owner));

  // Start the SessionManager, supplying this ChromotingHost as the listener.
  session_manager_->Init(signal_strategy_, this);
}

void ChromotingHost::AddStatusObserver(HostStatusObserver* observer) {
  DCHECK(CalledOnValidThread());
  status_observers_.AddObserver(observer);
}

void ChromotingHost::RemoveStatusObserver(HostStatusObserver* observer) {
  DCHECK(CalledOnValidThread());
  status_observers_.RemoveObserver(observer);
}

void ChromotingHost::AddExtension(scoped_ptr<HostExtension> extension) {
  extensions_.push_back(extension.release());
}

void ChromotingHost::RejectAuthenticatingClient() {
  DCHECK(authenticating_client_);
  reject_authenticating_client_ = true;
}

void ChromotingHost::SetAuthenticatorFactory(
    scoped_ptr<protocol::AuthenticatorFactory> authenticator_factory) {
  DCHECK(CalledOnValidThread());
  session_manager_->set_authenticator_factory(authenticator_factory.Pass());
}

void ChromotingHost::SetEnableCurtaining(bool enable) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (enable_curtaining_ == enable)
    return;

  enable_curtaining_ = enable;
  desktop_environment_factory_->SetEnableCurtaining(enable_curtaining_);

  // Disconnect all existing clients because they might be running not
  // curtained.
  // TODO(alexeypa): fix this such that the curtain is applied to the not
  // curtained sessions or disconnect only the client connected to not
  // curtained sessions.
  if (enable_curtaining_)
    DisconnectAllClients();
}

void ChromotingHost::SetMaximumSessionDuration(
    const base::TimeDelta& max_session_duration) {
  max_session_duration_ = max_session_duration;
}

////////////////////////////////////////////////////////////////////////////
// protocol::ClientSession::EventHandler implementation.
void ChromotingHost::OnSessionAuthenticating(ClientSession* client) {
  // We treat each incoming connection as a failure to authenticate,
  // and clear the backoff when a connection successfully
  // authenticates. This allows the backoff to protect from parallel
  // connection attempts as well as sequential ones.
  if (login_backoff_.ShouldRejectRequest()) {
    LOG(WARNING) << "Disconnecting client " << client->client_jid() << " due to"
                    " an overload of failed login attempts.";
    client->DisconnectSession();
    return;
  }
  login_backoff_.InformOfRequest(false);
}

bool ChromotingHost::OnSessionAuthenticated(ClientSession* client) {
  DCHECK(CalledOnValidThread());

  login_backoff_.Reset();

  // Disconnect all other clients. |it| should be advanced before Disconnect()
  // is called to avoid it becoming invalid when the client is removed from
  // the list.
  ClientList::iterator it = clients_.begin();
  while (it != clients_.end()) {
    ClientSession* other_client = *it++;
    if (other_client != client)
      other_client->DisconnectSession();
  }

  // Disconnects above must have destroyed all other clients.
  DCHECK_EQ(clients_.size(), 1U);

  // Notify observers that there is at least one authenticated client.
  const std::string& jid = client->client_jid();

  reject_authenticating_client_ = false;

  authenticating_client_ = true;
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientAuthenticated(jid));
  authenticating_client_ = false;

  return !reject_authenticating_client_;
}

void ChromotingHost::OnSessionChannelsConnected(ClientSession* client) {
  DCHECK(CalledOnValidThread());

  // Notify observers.
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientConnected(client->client_jid()));
}

void ChromotingHost::OnSessionAuthenticationFailed(ClientSession* client) {
  DCHECK(CalledOnValidThread());

  // Notify observers.
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnAccessDenied(client->client_jid()));
}

void ChromotingHost::OnSessionClosed(ClientSession* client) {
  DCHECK(CalledOnValidThread());

  ClientList::iterator it = std::find(clients_.begin(), clients_.end(), client);
  CHECK(it != clients_.end());

  if (client->is_authenticated()) {
    FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                      OnClientDisconnected(client->client_jid()));
  }

  clients_.erase(it);
  delete client;
}

void ChromotingHost::OnSessionRouteChange(
    ClientSession* session,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientRouteChange(session->client_jid(), channel_name,
                                        route));
}

void ChromotingHost::OnSessionManagerReady() {
  DCHECK(CalledOnValidThread());
  // Don't need to do anything here, just wait for incoming
  // connections.
}

void ChromotingHost::OnIncomingSession(
      protocol::Session* session,
      protocol::SessionManager::IncomingSessionResponse* response) {
  DCHECK(CalledOnValidThread());

  if (!started_) {
    *response = protocol::SessionManager::DECLINE;
    return;
  }

  if (login_backoff_.ShouldRejectRequest()) {
    LOG(WARNING) << "Rejecting connection due to"
                    " an overload of failed login attempts.";
    *response = protocol::SessionManager::OVERLOAD;
    return;
  }

  protocol::SessionConfig config;
  if (!protocol_config_->Select(session->candidate_config(), &config)) {
    LOG(WARNING) << "Rejecting connection from " << session->jid()
                 << " because no compatible configuration has been found.";
    *response = protocol::SessionManager::INCOMPATIBLE;
    return;
  }

  session->set_config(config);

  *response = protocol::SessionManager::ACCEPT;

  HOST_LOG << "Client connected: " << session->jid();

  // Create a client object.
  scoped_ptr<protocol::ConnectionToClient> connection(
      new protocol::ConnectionToClient(session));
  ClientSession* client = new ClientSession(
      this,
      audio_task_runner_,
      input_task_runner_,
      video_capture_task_runner_,
      video_encode_task_runner_,
      network_task_runner_,
      ui_task_runner_,
      connection.Pass(),
      desktop_environment_factory_,
      max_session_duration_,
      pairing_registry_,
      extensions_.get());

  clients_.push_back(client);
}

void ChromotingHost::set_protocol_config(
    scoped_ptr<protocol::CandidateSessionConfig> config) {
  DCHECK(CalledOnValidThread());
  DCHECK(config.get());
  DCHECK(!started_);
  protocol_config_ = config.Pass();
}

void ChromotingHost::DisconnectAllClients() {
  DCHECK(CalledOnValidThread());

  while (!clients_.empty()) {
    size_t size = clients_.size();
    clients_.front()->DisconnectSession();
    CHECK_EQ(clients_.size(), size - 1);
  }
}

}  // namespace remoting
