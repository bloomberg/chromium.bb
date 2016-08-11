// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/protocol/host_video_stats_dispatcher.h"
#include "remoting/protocol/video_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace webrtc {
class MediaStreamInterface;
class PeerConnectionInterface;
class PeerConnectionFactoryInterface;
class VideoTrackInterface;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class HostVideoStatsDispatcher;
class WebrtcVideoCapturerAdapter;
class WebrtcTransport;

class WebrtcVideoStream : public VideoStream,
                          public webrtc::DesktopCapturer::Callback,
                          public HostVideoStatsDispatcher::EventHandler {
 public:
  WebrtcVideoStream();
  ~WebrtcVideoStream() override;

  bool Start(
      std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
      WebrtcTransport* webrtc_transport,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
      std::unique_ptr<VideoEncoder> video_encoder);

  // VideoStream interface.
  void Pause(bool pause) override;
  void OnInputEventReceived(int64_t event_timestamp) override;
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void SetObserver(Observer* observer) override;

 private:
  struct FrameTimestamps;
  struct EncodedFrameWithTimestamps;

  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // HostVideoStatsDispatcher::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;
  void OnChannelClosed(ChannelDispatcherBase* channel_dispatcher) override;

  // Starts |capture_timer_|.
  void StartCaptureTimer();

  // Called by |capture_timer_|.
  void CaptureNextFrame();

  // Task running on the encoder thread to encode the |frame|.
  static EncodedFrameWithTimestamps EncodeFrame(
      VideoEncoder* encoder,
      std::unique_ptr<webrtc::DesktopFrame> frame,
      std::unique_ptr<WebrtcVideoStream::FrameTimestamps> timestamps,
      uint32_t target_bitrate_kbps,
      bool key_frame_request);
  void OnFrameEncoded(EncodedFrameWithTimestamps frame);

  void SetKeyFrameRequest();
  bool ClearAndGetKeyFrameRequest();
  void SetTargetBitrate(int bitrate);

  // Capturer used to capture the screen.
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
  // Used to send across encoded frames.
  WebrtcTransport* webrtc_transport_ = nullptr;
  // Task runner used to run |encoder_|.
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;
  // Used to encode captured frames. Always accessed on the encode thread.
  std::unique_ptr<VideoEncoder> encoder_;

  scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  scoped_refptr<webrtc::MediaStreamInterface> stream_;

  HostVideoStatsDispatcher video_stats_dispatcher_;

  // Timestamps for the frame to be captured next.
  std::unique_ptr<FrameTimestamps> next_frame_timestamps_;

  // Timestamps for the frame that's being captured.
  std::unique_ptr<FrameTimestamps> captured_frame_timestamps_;

  bool key_frame_request_ = false;
  uint32_t target_bitrate_kbps_ = 1000; // Initial bitrate.

  bool received_first_frame_request_ = false;

  bool capture_pending_ = false;
  bool encode_pending_ = false;

  // Last time capture was started.
  base::TimeTicks last_capture_started_ticks_;

  webrtc::DesktopSize frame_size_;
  webrtc::DesktopVector frame_dpi_;
  Observer* observer_ = nullptr;

  base::RepeatingTimer capture_timer_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<WebrtcVideoStream> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoStream);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
