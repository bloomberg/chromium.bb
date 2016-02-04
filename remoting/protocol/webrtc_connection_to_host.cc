// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_connection_to_host.h"

#include <utility>

#include "jingle/glue/thread_wrapper.h"
#include "remoting/protocol/client_control_dispatcher.h"
#include "remoting/protocol/client_event_dispatcher.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/protocol/webrtc_transport.h"
#include "remoting/protocol/webrtc_video_renderer_adapter.h"

namespace remoting {
namespace protocol {

WebrtcConnectionToHost::WebrtcConnectionToHost() {}
WebrtcConnectionToHost::~WebrtcConnectionToHost() {}

void WebrtcConnectionToHost::Connect(
    scoped_ptr<Session> session,
    scoped_refptr<TransportContext> transport_context,
    HostEventCallback* event_callback) {
  DCHECK(client_stub_);
  DCHECK(clipboard_stub_);

  transport_.reset(new WebrtcTransport(
      jingle_glue::JingleThreadWrapper::current(), transport_context, this));

  session_ = std::move(session);
  session_->SetEventHandler(this);
  session_->SetTransport(transport_.get());

  event_callback_ = event_callback;

  SetState(CONNECTING, OK);
}

const SessionConfig& WebrtcConnectionToHost::config() {
  return session_->config();
}

ClipboardStub* WebrtcConnectionToHost::clipboard_forwarder() {
  return &clipboard_forwarder_;
}

HostStub* WebrtcConnectionToHost::host_stub() {
  return control_dispatcher_.get();
}

InputStub* WebrtcConnectionToHost::input_stub() {
  return &event_forwarder_;
}

void WebrtcConnectionToHost::set_client_stub(ClientStub* client_stub) {
  client_stub_ = client_stub;
}

void WebrtcConnectionToHost::set_clipboard_stub(ClipboardStub* clipboard_stub) {
  clipboard_stub_ = clipboard_stub;
}

void WebrtcConnectionToHost::set_video_renderer(VideoRenderer* video_renderer) {
  video_renderer_ = video_renderer;
}

void WebrtcConnectionToHost::set_audio_stub(AudioStub* audio_stub) {
  NOTIMPLEMENTED();
}

void WebrtcConnectionToHost::OnSessionStateChange(Session::State state) {
  DCHECK(event_callback_);

  switch (state) {
    case Session::INITIALIZING:
    case Session::CONNECTING:
    case Session::ACCEPTING:
    case Session::ACCEPTED:
    case Session::AUTHENTICATING:
      // Don't care about these events.
      break;

    case Session::AUTHENTICATED:
      SetState(AUTHENTICATED, OK);
      break;

    case Session::CLOSED:
      CloseChannels();
      SetState(CLOSED,  OK);
      break;

    case Session::FAILED:
      CloseChannels();
      SetState(FAILED, session_->error());
      break;
  }
}

void WebrtcConnectionToHost::OnWebrtcTransportConnecting() {
  control_dispatcher_.reset(new ClientControlDispatcher());
  control_dispatcher_->Init(transport_->incoming_channel_factory(), this);
  control_dispatcher_->set_client_stub(client_stub_);
  control_dispatcher_->set_clipboard_stub(clipboard_stub_);

  event_dispatcher_.reset(new ClientEventDispatcher());
  event_dispatcher_->Init(transport_->outgoing_channel_factory(), this);
}

void WebrtcConnectionToHost::OnWebrtcTransportConnected() {}

void WebrtcConnectionToHost::OnWebrtcTransportError(ErrorCode error) {
  CloseChannels();
  SetState(FAILED, error);
}

void WebrtcConnectionToHost::OnWebrtcTransportMediaStreamAdded(
    scoped_refptr<webrtc::MediaStreamInterface> stream) {
  if (video_adapter_) {
    LOG(WARNING)
        << "Received multiple media streams. Ignoring all except the last one.";
  }
  video_adapter_.reset(new WebrtcVideoRendererAdapter(
      stream, video_renderer_->GetFrameConsumer()));
}

void WebrtcConnectionToHost::OnWebrtcTransportMediaStreamRemoved(
    scoped_refptr<webrtc::MediaStreamInterface> stream) {
  if (video_adapter_ && video_adapter_->label() == stream->label())
    video_adapter_.reset();
}

void WebrtcConnectionToHost::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  NotifyIfChannelsReady();
}

ConnectionToHost::State WebrtcConnectionToHost::state() const {
  return state_;
}

void WebrtcConnectionToHost::NotifyIfChannelsReady() {
  if (!control_dispatcher_.get() || !control_dispatcher_->is_connected())
    return;
  if (!event_dispatcher_.get() || !event_dispatcher_->is_connected())
    return;

  // Start forwarding clipboard and input events.
  clipboard_forwarder_.set_clipboard_stub(control_dispatcher_.get());
  event_forwarder_.set_input_stub(event_dispatcher_.get());
  SetState(CONNECTED, OK);
}

void WebrtcConnectionToHost::CloseChannels() {
  clipboard_forwarder_.set_clipboard_stub(nullptr);
  control_dispatcher_.reset();
  event_forwarder_.set_input_stub(nullptr);
  event_dispatcher_.reset();
}

void WebrtcConnectionToHost::SetState(State state, ErrorCode error) {
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
