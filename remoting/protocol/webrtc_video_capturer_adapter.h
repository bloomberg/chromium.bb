// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_CAPTURER_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_CAPTURER_ADAPTER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "third_party/webrtc/media/base/videocapturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace cricket {
class VideoFrame;
}  // namespace cricket

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {
namespace protocol {

// This class controls the capture of video frames from the desktop and is used
// to construct a VideoSource as part of the webrtc PeerConnection API.
// WebrtcVideoCapturerAdapter acts as an adapter between webrtc::DesktopCapturer
// and the cricket::VideoCapturer interface, which it implements. It is used
// to construct a cricket::VideoSource for a PeerConnection, to capture frames
// of the desktop. As indicated in the base implementation, Start() and Stop()
// should be called on the same thread.
class WebrtcVideoCapturerAdapter : public cricket::VideoCapturer,
                                   public webrtc::DesktopCapturer::Callback {
 public:
  typedef base::Callback<void(const webrtc::DesktopSize& size)> SizeCallback;

  explicit WebrtcVideoCapturerAdapter(
      scoped_ptr<webrtc::DesktopCapturer> capturer);
  ~WebrtcVideoCapturerAdapter() override;

  void SetSizeCallback(const SizeCallback& size_callback);
  base::WeakPtr<WebrtcVideoCapturerAdapter> GetWeakPtr();

  // cricket::VideoCapturer implementation.
  bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                            cricket::VideoFormat* best_format) override;
  cricket::CaptureState Start(
      const cricket::VideoFormat& capture_format) override;
  bool Pause(bool pause) override;
  void Stop() override;
  bool IsRunning() override;
  bool IsScreencast() const override;
  bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override;

 private:
  // webrtc::DesktopCapturer::Callback implementation.
  webrtc::SharedMemory* CreateSharedMemory(size_t size) override;
  void OnCaptureCompleted(webrtc::DesktopFrame* frame) override;

  // Kicks off the next frame capture using |desktop_capturer_|. The captured
  // frame will be passed to OnCaptureCompleted().
  void CaptureNextFrame();

  base::ThreadChecker thread_checker_;

  scoped_ptr<webrtc::DesktopCapturer> desktop_capturer_;

  SizeCallback size_callback_;

  // Timer to call CaptureNextFrame().
  scoped_ptr<base::RepeatingTimer> capture_timer_;

  // Video frame is kept between captures to avoid YUV conversion for static
  // parts of the screen.
  scoped_ptr<cricket::VideoFrame> yuv_frame_;

  bool capture_pending_ = false;

  base::WeakPtrFactory<WebrtcVideoCapturerAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoCapturerAdapter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_CAPTURER_ADAPTER_H_

