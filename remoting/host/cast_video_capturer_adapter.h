// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAST_VIDEO_CAPTURER_ADAPTER_H_
#define REMOTING_HOST_CAST_VIDEO_CAPTURER_ADAPTER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "third_party/libjingle/source/talk/media/base/videocapturer.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

// This class controls the capture of video frames from the desktop and is used
// to construct a VideoSource as part of the webrtc PeerConnection API.
// CastVideoCapturerAdapter acts as an adapter between webrtc::ScreenCapturer
// and the cricket::VideoCapturer interface, which it implements. It is used
// to construct a cricket::VideoSource for a PeerConnection, to capture frames
// of the desktop. As indicated in the base implementation, Start() and Stop()
// should be called on the same thread.
class CastVideoCapturerAdapter : public cricket::VideoCapturer,
                                 public webrtc::DesktopCapturer::Callback {
 public:
  explicit CastVideoCapturerAdapter(
      scoped_ptr<webrtc::ScreenCapturer> capturer);

  virtual ~CastVideoCapturerAdapter();

  // webrtc::DesktopCapturer::Callback implementation.
  virtual webrtc::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  // Converts |frame| to a cricket::CapturedFrame and emits that via
  // SignalFrameCaptured for the base::VideoCapturer implementation to process.
  virtual void OnCaptureCompleted(webrtc::DesktopFrame* frame) OVERRIDE;

  // cricket::VideoCapturer implementation.
  virtual bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                                  cricket::VideoFormat* best_format) OVERRIDE;
  virtual cricket::CaptureState Start(
      const cricket::VideoFormat& capture_format) OVERRIDE;
  virtual bool Pause(bool pause) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsRunning() OVERRIDE;
  virtual bool IsScreencast() const OVERRIDE;
  virtual bool GetPreferredFourccs(std::vector<uint32>* fourccs) OVERRIDE;

 private:
  // Kicks off the next frame capture using |screen_capturer_|.
  // The captured frame will be passed to OnCaptureCompleted().
  void CaptureNextFrame();

  // |thread_checker_| is bound to the peer connection worker thread.
  base::ThreadChecker thread_checker_;

  // Used to capture frames.
  scoped_ptr<webrtc::ScreenCapturer> screen_capturer_;

  // Used to schedule periodic screen captures.
  scoped_ptr<base::RepeatingTimer<CastVideoCapturerAdapter> > capture_timer_;

  // Used to set the elapsed_time attribute of captured frames.
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(CastVideoCapturerAdapter);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAST_VIDEO_CAPTURER_ADAPTER_H_

