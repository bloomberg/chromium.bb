// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_connection_to_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/audio_reader.h"
#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/client_control_dispatcher.h"
#include "remoting/protocol/client_event_dispatcher.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/client_video_dispatcher.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/ice_transport.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace protocol {

IceConnectionToHost::IceConnectionToHost() {}
IceConnectionToHost::~IceConnectionToHost() {}

void IceConnectionToHost::Connect(scoped_ptr<Session> session,
                                  HostEventCallback* event_callback) {
  DCHECK(client_stub_);
  DCHECK(clipboard_stub_);
  DCHECK(monitored_video_stub_);

  session_ = std::move(session);
  session_->SetEventHandler(this);

  event_callback_ = event_callback;

  SetState(CONNECTING, OK);
}

const SessionConfig& IceConnectionToHost::config() {
  return session_->config();
}

ClipboardStub* IceConnectionToHost::clipboard_forwarder() {
  return &clipboard_forwarder_;
}

HostStub* IceConnectionToHost::host_stub() {
  // TODO(wez): Add a HostFilter class, equivalent to input filter.
  return control_dispatcher_.get();
}

InputStub* IceConnectionToHost::input_stub() {
  return &event_forwarder_;
}

void IceConnectionToHost::set_client_stub(ClientStub* client_stub) {
  client_stub_ = client_stub;
}

void IceConnectionToHost::set_clipboard_stub(ClipboardStub* clipboard_stub) {
  clipboard_stub_ = clipboard_stub;
}

void IceConnectionToHost::set_video_stub(VideoStub* video_stub) {
  DCHECK(video_stub);
  monitored_video_stub_.reset(new MonitoredVideoStub(
      video_stub, base::TimeDelta::FromSeconds(
                      MonitoredVideoStub::kConnectivityCheckDelaySeconds),
      base::Bind(&IceConnectionToHost::OnVideoChannelStatus,
                 base::Unretained(this))));
}

void IceConnectionToHost::set_audio_stub(AudioStub* audio_stub) {
  audio_stub_ = audio_stub;
}

void IceConnectionToHost::OnSessionStateChange(Session::State state) {
  DCHECK(CalledOnValidThread());
  DCHECK(event_callback_);

  switch (state) {
    case Session::INITIALIZING:
    case Session::CONNECTING:
    case Session::ACCEPTING:
    case Session::ACCEPTED:
    case Session::AUTHENTICATING:
    case Session::CONNECTED:
      // Don't care about these events.
      break;

    case Session::AUTHENTICATED:
      SetState(AUTHENTICATED, OK);

      control_dispatcher_.reset(new ClientControlDispatcher());
      control_dispatcher_->Init(
          session_->GetTransport()->GetMultiplexedChannelFactory(), this);
      control_dispatcher_->set_client_stub(client_stub_);
      control_dispatcher_->set_clipboard_stub(clipboard_stub_);

      event_dispatcher_.reset(new ClientEventDispatcher());
      event_dispatcher_->Init(
          session_->GetTransport()->GetMultiplexedChannelFactory(), this);

      video_dispatcher_.reset(
          new ClientVideoDispatcher(monitored_video_stub_.get()));
      video_dispatcher_->Init(
          session_->GetTransport()->GetStreamChannelFactory(), this);

      if (session_->config().is_audio_enabled()) {
        audio_reader_.reset(new AudioReader(audio_stub_));
        audio_reader_->Init(
            session_->GetTransport()->GetMultiplexedChannelFactory(), this);
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

void IceConnectionToHost::OnSessionRouteChange(const std::string& channel_name,
                                               const TransportRoute& route) {
  event_callback_->OnRouteChanged(channel_name, route);
}

void IceConnectionToHost::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  NotifyIfChannelsReady();
}

void IceConnectionToHost::OnChannelError(
    ChannelDispatcherBase* channel_dispatcher,
    ErrorCode error) {
  LOG(ERROR) << "Failed to connect channel " << channel_dispatcher;
  CloseOnError(CHANNEL_CONNECTION_ERROR);
  return;
}

void IceConnectionToHost::OnVideoChannelStatus(bool active) {
  event_callback_->OnConnectionReady(active);
}

ConnectionToHost::State IceConnectionToHost::state() const {
  return state_;
}

void IceConnectionToHost::NotifyIfChannelsReady() {
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

void IceConnectionToHost::CloseOnError(ErrorCode error) {
  CloseChannels();
  SetState(FAILED, error);
}

void IceConnectionToHost::CloseChannels() {
  control_dispatcher_.reset();
  event_dispatcher_.reset();
  clipboard_forwarder_.set_clipboard_stub(nullptr);
  event_forwarder_.set_input_stub(nullptr);
  video_dispatcher_.reset();
  audio_reader_.reset();
}

void IceConnectionToHost::SetState(State state, ErrorCode error) {
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
