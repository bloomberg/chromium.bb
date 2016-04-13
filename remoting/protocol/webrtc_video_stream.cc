// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_stream.h"

#include "base/logging.h"
#include "remoting/protocol/webrtc_dummy_video_capturer.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/api/test/fakeconstraints.h"
#include "third_party/webrtc/media/base/videocapturer.h"

namespace remoting {
namespace protocol {

const char kStreamLabel[] = "screen_stream";
const char kVideoLabel[] = "screen_video";

WebrtcVideoStream::WebrtcVideoStream() {}

WebrtcVideoStream::~WebrtcVideoStream() {
  if (stream_) {
    for (const auto& track : stream_->GetVideoTracks()) {
      track->GetSource()->Stop();
      stream_->RemoveTrack(track.get());
    }
    connection_->RemoveStream(stream_.get());
  }
}

bool WebrtcVideoStream::Start(
    std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
    WebrtcTransport* webrtc_transport,
    scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
    std::unique_ptr<VideoEncoder> video_encoder) {
  DCHECK(webrtc_transport);
  DCHECK(desktop_capturer);
  DCHECK(video_encode_task_runner);
  DCHECK(video_encoder);

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory(
      webrtc_transport->peer_connection_factory());
  connection_ = webrtc_transport->peer_connection();
  DCHECK(peer_connection_factory);
  DCHECK(connection_);

  std::unique_ptr<WebRtcFrameScheduler> frame_scheduler(
      new WebRtcFrameScheduler(video_encode_task_runner,
                               std::move(desktop_capturer), webrtc_transport,
                               std::move(video_encoder)));
  webrtc_frame_scheduler_ = frame_scheduler.get();

  // Set video stream constraints.
  webrtc::FakeConstraints video_constraints;
  video_constraints.AddMandatory(
      webrtc::MediaConstraintsInterface::kMinFrameRate, 5);

  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> src =
      peer_connection_factory->CreateVideoSource(
          new WebRtcDummyVideoCapturer(std::move(frame_scheduler)),
          &video_constraints);
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      peer_connection_factory->CreateVideoTrack(kVideoLabel, src);

  stream_ = peer_connection_factory->CreateLocalMediaStream(kStreamLabel);

  if (!stream_->AddTrack(video_track.get()) ||
      !connection_->AddStream(stream_.get())) {
    stream_ = nullptr;
    connection_ = nullptr;
    return false;
  }
  return true;
}

void WebrtcVideoStream::Pause(bool pause) {
  if (webrtc_frame_scheduler_)
    webrtc_frame_scheduler_->Pause(pause);
}

void WebrtcVideoStream::OnInputEventReceived(int64_t event_timestamp) {
  NOTIMPLEMENTED();
}

void WebrtcVideoStream::SetLosslessEncode(bool want_lossless) {
  NOTIMPLEMENTED();
}

void WebrtcVideoStream::SetLosslessColor(bool want_lossless) {
  NOTIMPLEMENTED();
}

void WebrtcVideoStream::SetSizeCallback(const SizeCallback& size_callback) {
  if (webrtc_frame_scheduler_)
    webrtc_frame_scheduler_->SetSizeCallback(size_callback);
}

}  // namespace protocol
}  // namespace remoting
