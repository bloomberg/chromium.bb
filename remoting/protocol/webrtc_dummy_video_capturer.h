// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_DUMMY_VIDEO_CAPTURER_H_
#define REMOTING_PROTOCOL_WEBRTC_DUMMY_VIDEO_CAPTURER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "remoting/protocol/webrtc_frame_scheduler.h"
#include "third_party/webrtc/media/base/videocapturer.h"

namespace remoting {
namespace protocol {

// A dummy video capturer needed to create peer connection. We do not supply
// captured frames throught this interface, but instead provide encoded
// frames to WebRtc. We expect this requirement to go away once we have
// proper support for providing encoded frames to WebRtc through
// VideoSourceInterface
class WebRtcDummyVideoCapturer : public cricket::VideoCapturer {
 public:
  explicit WebRtcDummyVideoCapturer(
      std::unique_ptr<WebRtcFrameScheduler> frame_scheduler);
  ~WebRtcDummyVideoCapturer() override;

  cricket::CaptureState Start(
      const cricket::VideoFormat& capture_format) override;
  void Stop() override;
  bool IsRunning() override;
  bool IsScreencast() const override;
  bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override;

 private:
  std::unique_ptr<WebRtcFrameScheduler> frame_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcDummyVideoCapturer);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_DUMMY_VIDEO_CAPTURER_H_
