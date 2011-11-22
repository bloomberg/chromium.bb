// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_to_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/javascript_signal_strategy.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/auth_token_utils.h"
#include "remoting/protocol/client_control_dispatcher.h"
#include "remoting/protocol/client_event_dispatcher.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/pepper_session_manager.h"
#include "remoting/protocol/video_reader.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

ConnectionToHost::ConnectionToHost(
    base::MessageLoopProxy* message_loop,
    pp::Instance* pp_instance,
    bool allow_nat_traversal)
    : message_loop_(message_loop),
      pp_instance_(pp_instance),
      allow_nat_traversal_(allow_nat_traversal),
      event_callback_(NULL),
      client_stub_(NULL),
      video_stub_(NULL),
      state_(CONNECTING),
      error_(OK) {
}

ConnectionToHost::~ConnectionToHost() {
}

InputStub* ConnectionToHost::input_stub() {
  return event_dispatcher_.get();
}

HostStub* ConnectionToHost::host_stub() {
  return control_dispatcher_.get();
}

void ConnectionToHost::Connect(scoped_refptr<XmppProxy> xmpp_proxy,
                               const std::string& your_jid,
                               const std::string& host_jid,
                               const std::string& host_public_key,
                               const std::string& access_code,
                               HostEventCallback* event_callback,
                               ClientStub* client_stub,
                               VideoStub* video_stub) {
  event_callback_ = event_callback;
  client_stub_ = client_stub;
  video_stub_ = video_stub;
  access_code_ = access_code;

  // Save jid of the host. The actual connection is created later after
  // |signal_strategy_| is connected.
  host_jid_ = host_jid;
  host_public_key_ = host_public_key;

  JavascriptSignalStrategy* strategy = new JavascriptSignalStrategy(your_jid);
  strategy->AttachXmppProxy(xmpp_proxy);
  signal_strategy_.reset(strategy);
  signal_strategy_->Init(this);
}

void ConnectionToHost::Disconnect(const base::Closure& shutdown_task) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&ConnectionToHost::Disconnect,
                              base::Unretained(this), shutdown_task));
    return;
  }

  CloseChannels();

  if (session_.get())
    session_.reset();

  if (session_manager_.get())
    session_manager_.reset();

  if (signal_strategy_.get())
    signal_strategy_.reset();

  shutdown_task.Run();
}

void ConnectionToHost::InitSession() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  session_manager_.reset(new PepperSessionManager(pp_instance_));
  session_manager_->Init(
      local_jid_, signal_strategy_.get(), this, NULL, "", allow_nat_traversal_);
}

const SessionConfig& ConnectionToHost::config() {
  return session_->config();
}

void ConnectionToHost::OnStateChange(
    SignalStrategy::StatusObserver::State state) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(event_callback_);

  if (state == SignalStrategy::StatusObserver::CONNECTED) {
    VLOG(1) << "Connected as: " << local_jid_;
    InitSession();
  } else if (state == SignalStrategy::StatusObserver::CLOSED) {
    VLOG(1) << "Connection closed.";
    CloseOnError(NETWORK_FAILURE);
  }
}

void ConnectionToHost::OnJidChange(const std::string& full_jid) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  local_jid_ = full_jid;
}

void ConnectionToHost::OnSessionManagerInitialized() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // After SessionManager is initialized we can try to connect to the host.
  CandidateSessionConfig* candidate_config =
      CandidateSessionConfig::CreateDefault();
  std::string client_token =
      protocol::GenerateSupportAuthToken(local_jid_, access_code_);
  session_.reset(session_manager_->Connect(
      host_jid_, host_public_key_, client_token, candidate_config,
      base::Bind(&ConnectionToHost::OnSessionStateChange,
                 base::Unretained(this))));

  // Set the shared-secret for securing SSL channels.
  session_->set_shared_secret(access_code_);
}

void ConnectionToHost::OnIncomingSession(
    Session* session,
    SessionManager::IncomingSessionResponse* response) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  // Client always rejects incoming sessions.
  *response = SessionManager::DECLINE;
}

ConnectionToHost::State ConnectionToHost::state() const {
  return state_;
}

void ConnectionToHost::OnSessionStateChange(
    Session::State state) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(event_callback_);

  switch (state) {
    case Session::FAILED:
      switch (session_->error()) {
        case Session::PEER_IS_OFFLINE:
          CloseOnError(HOST_IS_OFFLINE);
          break;
        case Session::SESSION_REJECTED:
          CloseOnError(SESSION_REJECTED);
          break;
        case Session::INCOMPATIBLE_PROTOCOL:
          CloseOnError(INCOMPATIBLE_PROTOCOL);
          break;
        case Session::CHANNEL_CONNECTION_ERROR:
          CloseOnError(NETWORK_FAILURE);
          break;
        case Session::OK:
          DLOG(FATAL) << "Error code isn't set";
          CloseOnError(NETWORK_FAILURE);
      }
      break;

    case Session::CLOSED:
      CloseChannels();
      SetState(CLOSED, OK);
      break;

    case Session::CONNECTED:
      video_reader_.reset(VideoReader::Create(
          message_loop_, session_->config()));
      video_reader_->Init(session_.get(), video_stub_, base::Bind(
          &ConnectionToHost::OnChannelInitialized, base::Unretained(this)));

      control_dispatcher_.reset(new ClientControlDispatcher());
      control_dispatcher_->Init(session_.get(), base::Bind(
          &ConnectionToHost::OnChannelInitialized, base::Unretained(this)));
      control_dispatcher_->set_client_stub(client_stub_);

      event_dispatcher_.reset(new ClientEventDispatcher());
      event_dispatcher_->Init(session_.get(), base::Bind(
          &ConnectionToHost::OnChannelInitialized, base::Unretained(this)));
      break;

    default:
      // Ignore the other states by default.
      break;
  }
}

void ConnectionToHost::OnChannelInitialized(bool successful) {
  if (!successful) {
    LOG(ERROR) << "Failed to connect video channel";
    CloseOnError(NETWORK_FAILURE);
    return;
  }

  NotifyIfChannelsReady();
}

void ConnectionToHost::NotifyIfChannelsReady() {
  if (control_dispatcher_.get() && control_dispatcher_->is_connected() &&
      event_dispatcher_.get() && event_dispatcher_->is_connected() &&
      video_reader_.get() && video_reader_->is_connected() &&
      state_ == CONNECTING) {
    SetState(CONNECTED, OK);
    SetState(AUTHENTICATED, OK);
  }
}

void ConnectionToHost::CloseOnError(Error error) {
  CloseChannels();
  SetState(FAILED, error);
}

void ConnectionToHost::CloseChannels() {
  control_dispatcher_.reset();
  event_dispatcher_.reset();
  video_reader_.reset();
}

void ConnectionToHost::SetState(State state, Error error) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  // |error| should be specified only when |state| is set to FAILED.
  DCHECK(state == FAILED || error == OK);

  if (state != state_) {
    state_ = state;
    error_ = error;
    event_callback_->OnConnectionState(state_, error_);
  }
}

}  // namespace protocol
}  // namespace remoting
