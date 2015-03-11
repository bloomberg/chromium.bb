// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_to_host_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/audio_reader.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/client_control_dispatcher.h"
#include "remoting/protocol/client_event_dispatcher.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/client_video_dispatcher.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace protocol {

ConnectionToHostImpl::ConnectionToHostImpl()
    : event_callback_(nullptr),
      client_stub_(nullptr),
      clipboard_stub_(nullptr),
      audio_stub_(nullptr),
      signal_strategy_(nullptr),
      state_(INITIALIZING),
      error_(OK) {
}

ConnectionToHostImpl::~ConnectionToHostImpl() {
  CloseChannels();

  if (session_.get())
    session_.reset();

  if (session_manager_.get())
    session_manager_.reset();

  if (signal_strategy_)
    signal_strategy_->RemoveListener(this);
}

void ConnectionToHostImpl::Connect(
    SignalStrategy* signal_strategy,
    scoped_ptr<TransportFactory> transport_factory,
    scoped_ptr<Authenticator> authenticator,
    const std::string& host_jid,
    HostEventCallback* event_callback) {
  DCHECK(client_stub_);
  DCHECK(clipboard_stub_);
  DCHECK(monitored_video_stub_);

  // Initialize default |candidate_config_| if set_candidate_config() wasn't
  // called.
  if (!candidate_config_) {
    candidate_config_ = CandidateSessionConfig::CreateDefault();
    if (!audio_stub_) {
      candidate_config_->DisableAudioChannel();
    }
    candidate_config_->EnableVideoCodec(ChannelConfig::CODEC_VP9);
  }

  signal_strategy_ = signal_strategy;
  event_callback_ = event_callback;
  authenticator_ = authenticator.Pass();

  // Save jid of the host. The actual connection is created later after
  // |signal_strategy_| is connected.
  host_jid_ = host_jid;

  signal_strategy_->AddListener(this);
  signal_strategy_->Connect();

  session_manager_.reset(new JingleSessionManager(transport_factory.Pass()));
  session_manager_->Init(signal_strategy_, this);

  SetState(CONNECTING, OK);
}

void ConnectionToHostImpl::set_candidate_config(
    scoped_ptr<CandidateSessionConfig> config) {
  DCHECK_EQ(state_, INITIALIZING);

  candidate_config_ = config.Pass();
}

const SessionConfig& ConnectionToHostImpl::config() {
  return session_->config();
}

ClipboardStub* ConnectionToHostImpl::clipboard_forwarder() {
  return &clipboard_forwarder_;
}

HostStub* ConnectionToHostImpl::host_stub() {
  // TODO(wez): Add a HostFilter class, equivalent to input filter.
  return control_dispatcher_.get();
}

InputStub* ConnectionToHostImpl::input_stub() {
  return &event_forwarder_;
}

void ConnectionToHostImpl::set_client_stub(ClientStub* client_stub) {
  client_stub_ = client_stub;
}

void ConnectionToHostImpl::set_clipboard_stub(ClipboardStub* clipboard_stub) {
  clipboard_stub_ = clipboard_stub;
}

void ConnectionToHostImpl::set_video_stub(VideoStub* video_stub) {
  DCHECK(video_stub);
  monitored_video_stub_.reset(new MonitoredVideoStub(
      video_stub, base::TimeDelta::FromSeconds(
                      MonitoredVideoStub::kConnectivityCheckDelaySeconds),
      base::Bind(&ConnectionToHostImpl::OnVideoChannelStatus,
                 base::Unretained(this))));
}

void ConnectionToHostImpl::set_audio_stub(AudioStub* audio_stub) {
  audio_stub_ = audio_stub;
}

void ConnectionToHostImpl::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  DCHECK(CalledOnValidThread());
  DCHECK(event_callback_);

  if (state == SignalStrategy::CONNECTED) {
    VLOG(1) << "Connected as: " << signal_strategy_->GetLocalJid();
  } else if (state == SignalStrategy::DISCONNECTED) {
    VLOG(1) << "Connection closed.";
    CloseOnError(SIGNALING_ERROR);
  }
}

bool ConnectionToHostImpl::OnSignalStrategyIncomingStanza(
    const buzz::XmlElement* stanza) {
  return false;
}

void ConnectionToHostImpl::OnSessionManagerReady() {
  DCHECK(CalledOnValidThread());

  // After SessionManager is initialized we can try to connect to the host.
  session_ = session_manager_->Connect(host_jid_, authenticator_.Pass(),
                                       candidate_config_.Pass());
  session_->SetEventHandler(this);
}

void ConnectionToHostImpl::OnIncomingSession(
    Session* session,
    SessionManager::IncomingSessionResponse* response) {
  DCHECK(CalledOnValidThread());
  // Client always rejects incoming sessions.
  *response = SessionManager::DECLINE;
}

void ConnectionToHostImpl::OnSessionStateChange(Session::State state) {
  DCHECK(CalledOnValidThread());
  DCHECK(event_callback_);

  switch (state) {
    case Session::INITIALIZING:
    case Session::CONNECTING:
    case Session::ACCEPTING:
    case Session::CONNECTED:
    case Session::AUTHENTICATING:
      // Don't care about these events.
      break;

    case Session::AUTHENTICATED:
      SetState(AUTHENTICATED, OK);

      control_dispatcher_.reset(new ClientControlDispatcher());
      control_dispatcher_->Init(session_.get(),
                                session_->config().control_config(), this);
      control_dispatcher_->set_client_stub(client_stub_);
      control_dispatcher_->set_clipboard_stub(clipboard_stub_);

      event_dispatcher_.reset(new ClientEventDispatcher());
      event_dispatcher_->Init(session_.get(), session_->config().event_config(),
                              this);

      video_dispatcher_.reset(
          new ClientVideoDispatcher(monitored_video_stub_.get()));
      video_dispatcher_->Init(session_.get(), session_->config().video_config(),
                              this);

      if (session_->config().is_audio_enabled()) {
        audio_reader_.reset(new AudioReader(audio_stub_));
        audio_reader_->Init(session_.get(), session_->config().audio_config(),
                            this);
      }
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

void ConnectionToHostImpl::OnSessionRouteChange(const std::string& channel_name,
                                                const TransportRoute& route) {
  event_callback_->OnRouteChanged(channel_name, route);
}

void ConnectionToHostImpl::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  NotifyIfChannelsReady();
}

void ConnectionToHostImpl::OnChannelError(
    ChannelDispatcherBase* channel_dispatcher,
    ErrorCode error) {
  LOG(ERROR) << "Failed to connect channel " << channel_dispatcher;
  CloseOnError(CHANNEL_CONNECTION_ERROR);
  return;
}

void ConnectionToHostImpl::OnVideoChannelStatus(bool active) {
  event_callback_->OnConnectionReady(active);
}

ConnectionToHost::State ConnectionToHostImpl::state() const {
  return state_;
}

void ConnectionToHostImpl::NotifyIfChannelsReady() {
  if (!control_dispatcher_.get() || !control_dispatcher_->is_connected())
    return;
  if (!event_dispatcher_.get() || !event_dispatcher_->is_connected())
    return;
  if (!video_dispatcher_.get() || !video_dispatcher_->is_connected())
    return;
  if ((!audio_reader_.get() || !audio_reader_->is_connected()) &&
      session_->config().is_audio_enabled()) {
    return;
  }
  if (state_ != AUTHENTICATED)
    return;

  // Start forwarding clipboard and input events.
  clipboard_forwarder_.set_clipboard_stub(control_dispatcher_.get());
  event_forwarder_.set_input_stub(event_dispatcher_.get());
  SetState(CONNECTED, OK);
}

void ConnectionToHostImpl::CloseOnError(ErrorCode error) {
  CloseChannels();
  SetState(FAILED, error);
}

void ConnectionToHostImpl::CloseChannels() {
  control_dispatcher_.reset();
  event_dispatcher_.reset();
  clipboard_forwarder_.set_clipboard_stub(nullptr);
  event_forwarder_.set_input_stub(nullptr);
  video_dispatcher_.reset();
  audio_reader_.reset();
}

void ConnectionToHostImpl::SetState(State state, ErrorCode error) {
  DCHECK(CalledOnValidThread());
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
