// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_

#include "base/macros.h"
#include "remoting/protocol/video_stream.h"
#include "third_party/webrtc/base/scoped_ref_ptr.h"

namespace webrtc {
class MediaStreamInterface;
class PeerConnectionInterface;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class WebrtcVideoStream : public VideoStream {
 public:
  WebrtcVideoStream(
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> connection,
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
  ~WebrtcVideoStream() override;

  // VideoStream interface.
  void Pause(bool pause) override;
  void OnInputEventReceived(int64_t event_timestamp) override;
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void SetSizeCallback(const SizeCallback& size_callback) override;

 private:
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> connection_;
  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoStream);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
