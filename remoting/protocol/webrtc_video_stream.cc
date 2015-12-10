// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_stream.h"

#include "base/logging.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/videosourceinterface.h"

namespace remoting {
namespace protocol {

WebrtcVideoStream::WebrtcVideoStream(
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> connection,
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
    : connection_(connection), stream_(stream) {}

WebrtcVideoStream::~WebrtcVideoStream() {
  for (const auto& track : stream_->GetVideoTracks()) {
    track->GetSource()->Stop();
    stream_->RemoveTrack(track.get());
  }
  connection_->RemoveStream(stream_.get());
}

void WebrtcVideoStream::Pause(bool pause) {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
}

}  // namespace protocol
}  // namespace remoting
