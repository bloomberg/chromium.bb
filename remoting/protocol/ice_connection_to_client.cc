// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_connection_to_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "net/base/io_buffer.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/codec/video_encoder_vpx.h"
#include "remoting/protocol/audio_writer.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/host_control_dispatcher.h"
#include "remoting/protocol/host_event_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/host_video_dispatcher.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_frame_pump.h"

namespace remoting {
namespace protocol {

namespace {

scoped_ptr<VideoEncoder> CreateVideoEncoder(
    const protocol::SessionConfig& config) {
  const protocol::ChannelConfig& video_config = config.video_config();

  if (video_config.codec == protocol::ChannelConfig::CODEC_VP8) {
    return VideoEncoderVpx::CreateForVP8();
  } else if (video_config.codec == protocol::ChannelConfig::CODEC_VP9) {
    return VideoEncoderVpx::CreateForVP9();
  } else if (video_config.codec == protocol::ChannelConfig::CODEC_VERBATIM) {
    return make_scoped_ptr(new VideoEncoderVerbatim());
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace

IceConnectionToClient::IceConnectionToClient(
    scoped_ptr<protocol::Session> session,
    scoped_refptr<TransportContext> transport_context,
    scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner)
    : event_handler_(nullptr),
      session_(std::move(session)),
      video_encode_task_runner_(video_encode_task_runner),
      transport_(transport_context, this),
      control_dispatcher_(new HostControlDispatcher()),
      event_dispatcher_(new HostEventDispatcher()),
      video_dispatcher_(new HostVideoDispatcher()) {
  session_->SetEventHandler(this);
  session_->SetTransport(&transport_);
}

IceConnectionToClient::~IceConnectionToClient() {}

void IceConnectionToClient::SetEventHandler(
    ConnectionToClient::EventHandler* event_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_handler_ = event_handler;
}

protocol::Session* IceConnectionToClient::session() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return session_.get();
}

void IceConnectionToClient::Disconnect(ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This should trigger OnConnectionClosed() event and this object
  // may be destroyed as the result.
  session_->Close(error);
}

void IceConnectionToClient::OnInputEventReceived(int64_t timestamp) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_handler_->OnInputEventReceived(this, timestamp);
}

scoped_ptr<VideoStream> IceConnectionToClient::StartVideoStream(
    scoped_ptr<webrtc::DesktopCapturer> desktop_capturer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<VideoEncoder> video_encoder =
      CreateVideoEncoder(session_->config());
  event_handler_->OnCreateVideoEncoder(&video_encoder);
  DCHECK(video_encoder);

  scoped_ptr<VideoFramePump> pump(
      new VideoFramePump(video_encode_task_runner_, std::move(desktop_capturer),
                         std::move(video_encoder), video_dispatcher_.get()));
  video_dispatcher_->set_video_feedback_stub(pump->video_feedback_stub());
  return std::move(pump);
}

AudioStub* IceConnectionToClient::audio_stub() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return audio_writer_.get();
}

// Return pointer to ClientStub.
ClientStub* IceConnectionToClient::client_stub() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return control_dispatcher_.get();
}

void IceConnectionToClient::set_clipboard_stub(
    protocol::ClipboardStub* clipboard_stub) {
  DCHECK(thread_checker_.CalledOnValidThread());
  control_dispatcher_->set_clipboard_stub(clipboard_stub);
}

void IceConnectionToClient::set_host_stub(protocol::HostStub* host_stub) {
  DCHECK(thread_checker_.CalledOnValidThread());
  control_dispatcher_->set_host_stub(host_stub);
}

void IceConnectionToClient::set_input_stub(protocol::InputStub* input_stub) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_dispatcher_->set_input_stub(input_stub);
}

void IceConnectionToClient::OnSessionStateChange(Session::State state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(event_handler_);
  switch (state) {
    case Session::INITIALIZING:
    case Session::CONNECTING:
    case Session::ACCEPTING:
    case Session::ACCEPTED:
      // Don't care about these events.
      break;
    case Session::AUTHENTICATING:
      event_handler_->OnConnectionAuthenticating(this);
      break;
    case Session::AUTHENTICATED:
      // Initialize channels.
      control_dispatcher_->Init(transport_.GetMultiplexedChannelFactory(),
                                this);

      event_dispatcher_->Init(transport_.GetMultiplexedChannelFactory(), this);
      event_dispatcher_->set_on_input_event_callback(
          base::Bind(&IceConnectionToClient::OnInputEventReceived,
                     base::Unretained(this)));

      video_dispatcher_->Init(transport_.GetChannelFactory(), this);

      audio_writer_ = AudioWriter::Create(session_->config());
      if (audio_writer_)
        audio_writer_->Init(transport_.GetMultiplexedChannelFactory(), this);

      // Notify the handler after initializing the channels, so that
      // ClientSession can get a client clipboard stub.
      event_handler_->OnConnectionAuthenticated(this);
      break;

    case Session::CLOSED:
    case Session::FAILED:
      CloseChannels();
      event_handler_->OnConnectionClosed(
          this, state == Session::FAILED ? session_->error() : OK);
      break;
  }
}


void IceConnectionToClient::OnIceTransportRouteChange(
    const std::string& channel_name,
    const TransportRoute& route) {
  event_handler_->OnRouteChange(this, channel_name, route);
}

void IceConnectionToClient::OnIceTransportError(ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Disconnect(error);
}

void IceConnectionToClient::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK(thread_checker_.CalledOnValidThread());

  NotifyIfChannelsReady();
}

void IceConnectionToClient::NotifyIfChannelsReady() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!control_dispatcher_ || !control_dispatcher_->is_connected())
    return;
  if (!event_dispatcher_ || !event_dispatcher_->is_connected())
    return;
  if (!video_dispatcher_ || !video_dispatcher_->is_connected())
    return;
  if ((!audio_writer_ || !audio_writer_->is_connected()) &&
      session_->config().is_audio_enabled()) {
    return;
  }
  event_handler_->OnConnectionChannelsConnected(this);
}

void IceConnectionToClient::CloseChannels() {
  control_dispatcher_.reset();
  event_dispatcher_.reset();
  video_dispatcher_.reset();
  audio_writer_.reset();
}

}  // namespace protocol
}  // namespace remoting
