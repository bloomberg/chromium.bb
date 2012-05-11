// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/client_control_dispatcher.h"
#include "remoting/protocol/client_event_dispatcher.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/pepper_transport_factory.h"
#include "remoting/protocol/video_reader.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

ConnectionToHost::ConnectionToHost(
    base::MessageLoopProxy* message_loop,
    bool allow_nat_traversal)
    : message_loop_(message_loop),
      allow_nat_traversal_(allow_nat_traversal),
      event_callback_(NULL),
      client_stub_(NULL),
      clipboard_stub_(NULL),
      video_stub_(NULL),
      state_(CONNECTING),
      error_(OK) {
}

ConnectionToHost::~ConnectionToHost() {
}

ClipboardStub* ConnectionToHost::clipboard_stub() {
  return &clipboard_forwarder_;
}

HostStub* ConnectionToHost::host_stub() {
  // TODO(wez): Add a HostFilter class, equivalent to input filter.
  return control_dispatcher_.get();
}

InputStub* ConnectionToHost::input_stub() {
  return &event_forwarder_;
}

void ConnectionToHost::Connect(scoped_refptr<XmppProxy> xmpp_proxy,
                               const std::string& local_jid,
                               const std::string& host_jid,
                               const std::string& host_public_key,
                               scoped_ptr<TransportFactory> transport_factory,
                               scoped_ptr<Authenticator> authenticator,
                               HostEventCallback* event_callback,
                               ClientStub* client_stub,
                               ClipboardStub* clipboard_stub,
                               VideoStub* video_stub) {
  event_callback_ = event_callback;
  client_stub_ = client_stub;
  clipboard_stub_ = clipboard_stub;
  video_stub_ = video_stub;
  authenticator_ = authenticator.Pass();

  // Save jid of the host. The actual connection is created later after
  // |signal_strategy_| is connected.
  host_jid_ = host_jid;
  host_public_key_ = host_public_key;

  JavascriptSignalStrategy* strategy = new JavascriptSignalStrategy(local_jid);
  strategy->AttachXmppProxy(xmpp_proxy);
  signal_strategy_.reset(strategy);
  signal_strategy_->AddListener(this);
  signal_strategy_->Connect();

  session_manager_.reset(new JingleSessionManager(
      transport_factory.Pass(), allow_nat_traversal_));
  session_manager_->Init(signal_strategy_.get(), this);
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

  if (signal_strategy_.get()) {
    signal_strategy_->RemoveListener(this);
    signal_strategy_.reset();
  }

  shutdown_task.Run();
}

const SessionConfig& ConnectionToHost::config() {
  return session_->config();
}

void ConnectionToHost::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(event_callback_);

  if (state == SignalStrategy::CONNECTED) {
    VLOG(1) << "Connected as: " << signal_strategy_->GetLocalJid();
  } else if (state == SignalStrategy::DISCONNECTED) {
    VLOG(1) << "Connection closed.";
    CloseOnError(SIGNALING_ERROR);
  }
}

void ConnectionToHost::OnSessionManagerReady() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // After SessionManager is initialized we can try to connect to the host.
  scoped_ptr<CandidateSessionConfig> candidate_config =
      CandidateSessionConfig::CreateDefault();
  session_ = session_manager_->Connect(
      host_jid_, authenticator_.Pass(), candidate_config.Pass(),
      base::Bind(&ConnectionToHost::OnSessionStateChange,
                 base::Unretained(this)));
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
    case Session::INITIALIZING:
    case Session::CONNECTING:
    case Session::CONNECTED:
      // Don't care about these events.
      break;

    case Session::AUTHENTICATED:
      video_reader_.reset(VideoReader::Create(
          message_loop_, session_->config()));
      video_reader_->Init(session_.get(), video_stub_, base::Bind(
          &ConnectionToHost::OnChannelInitialized, base::Unretained(this)));

      control_dispatcher_.reset(new ClientControlDispatcher());
      control_dispatcher_->Init(session_.get(), base::Bind(
          &ConnectionToHost::OnChannelInitialized, base::Unretained(this)));
      control_dispatcher_->set_client_stub(client_stub_);
      control_dispatcher_->set_clipboard_stub(clipboard_stub_);

      event_dispatcher_.reset(new ClientEventDispatcher());
      event_dispatcher_->Init(session_.get(), base::Bind(
          &ConnectionToHost::OnChannelInitialized, base::Unretained(this)));
      break;

    case Session::CLOSED:
      CloseChannels();
      SetState(CLOSED, OK);
      break;

    case Session::FAILED:
      // If we were connected then treat signaling timeout error as if
      // the connection was closed by the peer.
      //
      // TODO(sergeyu): This logic belongs to the webapp, but we
      // currently don't expose this error code to the webapp, and it
      // would be hard to add it because client plugin and webapp
      // versions may not be in sync. It should be easy to do after we
      // are finished moving the client plugin to NaCl.
      if (state_ == CONNECTED && session_->error() == SIGNALING_TIMEOUT) {
        CloseChannels();
        SetState(CLOSED, OK);
      } else {
        CloseOnError(session_->error());
      }
      break;
  }
}

void ConnectionToHost::OnChannelInitialized(bool successful) {
  if (!successful) {
    LOG(ERROR) << "Failed to connect video channel";
    CloseOnError(CHANNEL_CONNECTION_ERROR);
    return;
  }

  NotifyIfChannelsReady();
}

void ConnectionToHost::NotifyIfChannelsReady() {
  if (control_dispatcher_.get() && control_dispatcher_->is_connected() &&
      event_dispatcher_.get() && event_dispatcher_->is_connected() &&
      video_reader_.get() && video_reader_->is_connected() &&
      state_ == CONNECTING) {
    // Start forwarding clipboard and input events.
    clipboard_forwarder_.set_clipboard_stub(control_dispatcher_.get());
    event_forwarder_.set_input_stub(event_dispatcher_.get());
    SetState(CONNECTED, OK);
  }
}

void ConnectionToHost::CloseOnError(ErrorCode error) {
  CloseChannels();
  SetState(FAILED, error);
}

void ConnectionToHost::CloseChannels() {
  control_dispatcher_.reset();
  event_dispatcher_.reset();
  clipboard_forwarder_.set_clipboard_stub(NULL);
  event_forwarder_.set_input_stub(NULL);
  video_reader_.reset();
}

void ConnectionToHost::SetState(State state, ErrorCode error) {
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
