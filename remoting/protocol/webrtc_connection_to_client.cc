// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_connection_to_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/io_buffer.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/codec/video_encoder_vpx.h"
#include "remoting/protocol/audio_writer.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/host_control_dispatcher.h"
#include "remoting/protocol/host_event_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/webrtc_transport.h"
#include "remoting/protocol/webrtc_video_capturer_adapter.h"
#include "remoting/protocol/webrtc_video_stream.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/test/fakeconstraints.h"
#include "third_party/libjingle/source/talk/app/webrtc/videosourceinterface.h"

namespace remoting {
namespace protocol {

const char kStreamLabel[] = "screen_stream";
const char kVideoLabel[] = "screen_video";

// Currently the network thread is also used as worker thread for webrtc.
//
// TODO(sergeyu): Figure out if we would benefit from using a separate
// thread as a worker thread.
WebrtcConnectionToClient::WebrtcConnectionToClient(
    scoped_ptr<protocol::Session> session,
    scoped_refptr<protocol::TransportContext> transport_context)
    : transport_(jingle_glue::JingleThreadWrapper::current(),
                 transport_context,
                 this),
      session_(std::move(session)),
      control_dispatcher_(new HostControlDispatcher()),
      event_dispatcher_(new HostEventDispatcher()) {
  session_->SetEventHandler(this);
  session_->SetTransport(&transport_);
}

WebrtcConnectionToClient::~WebrtcConnectionToClient() {}

void WebrtcConnectionToClient::SetEventHandler(
    ConnectionToClient::EventHandler* event_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_handler_ = event_handler;
}

protocol::Session* WebrtcConnectionToClient::session() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return session_.get();
}

void WebrtcConnectionToClient::Disconnect(ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This should trigger OnConnectionClosed() event and this object
  // may be destroyed as the result.
  session_->Close(error);
}

void WebrtcConnectionToClient::OnInputEventReceived(int64_t timestamp) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_handler_->OnInputEventReceived(this, timestamp);
}

scoped_ptr<VideoStream> WebrtcConnectionToClient::StartVideoStream(
    scoped_ptr<webrtc::DesktopCapturer> desktop_capturer) {
  scoped_ptr<WebrtcVideoCapturerAdapter> video_capturer_adapter(
      new WebrtcVideoCapturerAdapter(std::move(desktop_capturer)));

  // Set video stream constraints.
  webrtc::FakeConstraints video_constraints;
  video_constraints.AddMandatory(
      webrtc::MediaConstraintsInterface::kMinFrameRate, 5);

  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      transport_.peer_connection_factory()->CreateVideoTrack(
          kVideoLabel,
          transport_.peer_connection_factory()->CreateVideoSource(
              video_capturer_adapter.release(), &video_constraints));

  rtc::scoped_refptr<webrtc::MediaStreamInterface> video_stream =
      transport_.peer_connection_factory()->CreateLocalMediaStream(
          kStreamLabel);

  if (!video_stream->AddTrack(video_track) ||
      !transport_.peer_connection()->AddStream(video_stream)) {
    return nullptr;
  }

  return make_scoped_ptr(
      new WebrtcVideoStream(transport_.peer_connection(), video_stream));
}

AudioStub* WebrtcConnectionToClient::audio_stub() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

// Return pointer to ClientStub.
ClientStub* WebrtcConnectionToClient::client_stub() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return control_dispatcher_.get();
}

void WebrtcConnectionToClient::set_clipboard_stub(
    protocol::ClipboardStub* clipboard_stub) {
  DCHECK(thread_checker_.CalledOnValidThread());
  control_dispatcher_->set_clipboard_stub(clipboard_stub);
}

void WebrtcConnectionToClient::set_host_stub(protocol::HostStub* host_stub) {
  DCHECK(thread_checker_.CalledOnValidThread());
  control_dispatcher_->set_host_stub(host_stub);
}

void WebrtcConnectionToClient::set_input_stub(protocol::InputStub* input_stub) {
  DCHECK(thread_checker_.CalledOnValidThread());
  event_dispatcher_->set_input_stub(input_stub);
}

void WebrtcConnectionToClient::OnSessionStateChange(Session::State state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(event_handler_);
  switch(state) {
    case Session::INITIALIZING:
    case Session::CONNECTING:
    case Session::ACCEPTING:
    case Session::ACCEPTED:
      // Don't care about these events.
      break;
    case Session::AUTHENTICATING:
      event_handler_->OnConnectionAuthenticating(this);
      break;
    case Session::AUTHENTICATED: {
      // Initialize channels.
      control_dispatcher_->Init(transport_.GetStreamChannelFactory(), this);

      event_dispatcher_->Init(transport_.GetStreamChannelFactory(), this);
      event_dispatcher_->set_on_input_event_callback(base::Bind(
          &ConnectionToClient::OnInputEventReceived, base::Unretained(this)));

      // Notify the handler after initializing the channels, so that
      // ClientSession can get a client clipboard stub.
      event_handler_->OnConnectionAuthenticated(this);
      break;
    }

    case Session::CLOSED:
    case Session::FAILED:
      control_dispatcher_.reset();
      event_dispatcher_.reset();
      event_handler_->OnConnectionClosed(
          this, state == Session::CLOSED ? OK : session_->error());
      break;
  }
}

void WebrtcConnectionToClient::OnWebrtcTransportConnected() {
  event_handler_->OnConnectionChannelsConnected(this);
}

void WebrtcConnectionToClient::OnWebrtcTransportError(ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Disconnect(error);
}

void WebrtcConnectionToClient::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcConnectionToClient::OnChannelError(
    ChannelDispatcherBase* channel_dispatcher,
    ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  LOG(ERROR) << "Failed to connect channel "
             << channel_dispatcher->channel_name();
  Disconnect(error);
}

}  // namespace protocol
}  // namespace remoting
