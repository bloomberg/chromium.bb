// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/protocol/video_stream.h"
#include "remoting/protocol/webrtc_frame_scheduler.h"

namespace webrtc {
class DesktopSize;
class DesktopCapturer;
class MediaStreamInterface;
class PeerConnectionInterface;
class PeerConnectionFactoryInterface;
class VideoTrackInterface;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class WebrtcVideoCapturerAdapter;

class WebrtcVideoStream : public VideoStream {
 public:
  WebrtcVideoStream();
  ~WebrtcVideoStream() override;

  bool Start(
      std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
      WebrtcTransport* webrtc_transport,
      scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner,
      std::unique_ptr<VideoEncoder> video_encoder);

  // VideoStream interface.
  void Pause(bool pause) override;
  void OnInputEventReceived(int64_t event_timestamp) override;
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void SetSizeCallback(const SizeCallback& size_callback) override;

 private:
  scoped_refptr<webrtc::PeerConnectionInterface> connection_;
  scoped_refptr<webrtc::MediaStreamInterface> stream_;

  // Owned by the dummy video capturer.
  WebRtcFrameScheduler* webrtc_frame_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoStream);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
