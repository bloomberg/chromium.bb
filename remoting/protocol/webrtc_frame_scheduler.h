// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/protocol/video_stream.h"
#include "remoting/protocol/webrtc_transport.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

// Class responsible for scheduling frames for encode and hand-over to
// WebRtc transport for sending across the network.
class WebRtcFrameScheduler : public webrtc::DesktopCapturer::Callback {
 public:
  WebRtcFrameScheduler(
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
      std::unique_ptr<webrtc::DesktopCapturer> capturer,
      WebrtcTransport* webrtc_transport,
      std::unique_ptr<VideoEncoder> encoder);
  ~WebRtcFrameScheduler() override;

  // Starts scheduling frames to be encoded and transported.
  void Start();
  // Stops scheduling frames.
  void Stop();

  void Pause(bool pause);

  void SetSizeCallback(const VideoStream::SizeCallback& size_callback);

 private:
  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureCompleted(webrtc::DesktopFrame* frame) override;

  // Callback for CaptureScheduler.
  void CaptureNextFrame();

  // Task running on the encoder thread to encode the |frame|.
  static std::unique_ptr<VideoPacket> EncodeFrame(
      VideoEncoder* encoder,
      std::unique_ptr<webrtc::DesktopFrame> frame,
      bool key_frame_request);
  void OnFrameEncoded(std::unique_ptr<VideoPacket> packet);

  void SetKeyFrameRequest();
  bool ClearAndGetKeyFrameRequest();

  // Protects |key_frame_request_|.
  base::Lock lock_;
  bool key_frame_request_ = false;

  bool capture_pending_ = false;
  bool encode_pending_ = false;

  webrtc::DesktopSize frame_size_;
  webrtc::DesktopVector frame_dpi_;
  VideoStream::SizeCallback size_callback_;

  // Task runner used to run |encoder_|.
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;
  base::CancelableTaskTracker task_tracker_;

  // Capturer used to capture the screen.
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;

  std::unique_ptr<base::RepeatingTimer> capture_timer_;

  // Used to send across encoded frames.
  WebrtcTransport* webrtc_transport_;

  // Used to encode captured frames. Always accessed on the encode thread.
  std::unique_ptr<VideoEncoder> encoder_;

  base::ThreadChecker thread_checker_;
  base::TimeTicks last_capture_ticks_;

  base::WeakPtrFactory<WebRtcFrameScheduler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcFrameScheduler);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_
