// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_stream.h"

#include "base/logging.h"
#include "remoting/protocol/webrtc_video_capturer_adapter.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/api/test/fakeconstraints.h"
#include "third_party/webrtc/api/videosourceinterface.h"

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

  // MediaStream may still outlive WebrtcVideoStream because it's
  // ref-counted. Reset SizeCallback to make sure it won't be called again.
  if (capturer_adapter_)
    capturer_adapter_->SetSizeCallback(SizeCallback());
}

bool WebrtcVideoStream::Start(
    scoped_ptr<webrtc::DesktopCapturer> desktop_capturer,
    scoped_refptr<webrtc::PeerConnectionInterface> connection,
    scoped_refptr<webrtc::PeerConnectionFactoryInterface>
        peer_connection_factory) {
  scoped_ptr<WebrtcVideoCapturerAdapter> capturer_adapter(
      new WebrtcVideoCapturerAdapter(std::move(desktop_capturer)));
  capturer_adapter_ = capturer_adapter->GetWeakPtr();

  connection_ = connection;

  // Set video stream constraints.
  webrtc::FakeConstraints video_constraints;
  video_constraints.AddMandatory(
      webrtc::MediaConstraintsInterface::kMinFrameRate, 5);

  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      peer_connection_factory->CreateVideoTrack(
          kVideoLabel, peer_connection_factory->CreateVideoSource(
                           capturer_adapter.release(), &video_constraints));

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
  if (capturer_adapter_)
    capturer_adapter_->Pause(pause);
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
  if (capturer_adapter_)
    capturer_adapter_->SetSizeCallback(size_callback);
}

}  // namespace protocol
}  // namespace remoting
