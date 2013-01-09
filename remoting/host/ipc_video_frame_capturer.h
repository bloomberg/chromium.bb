// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
#define REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_

#include "base/memory/ref_counted.h"
#include "remoting/capturer/video_frame_capturer.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace remoting {

class DesktopSessionProxy;
struct MouseCursorShape;

// Routes VideoFrameCapturer calls though the IPC channel to the desktop session
// agent running in the desktop integration process.
class IpcVideoFrameCapturer : public VideoFrameCapturer {
 public:
  explicit IpcVideoFrameCapturer(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);
  virtual ~IpcVideoFrameCapturer();

  // VideoFrameCapturer interface.
  virtual void Start(Delegate* delegate) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void InvalidateRegion(const SkRegion& invalid_region) OVERRIDE;
  virtual void CaptureFrame() OVERRIDE;

  // Called when a video frame has been captured. |capture_data| describes
  // a captured frame.
  void OnCaptureCompleted(scoped_refptr<CaptureData> capture_data);

  // Called when the cursor shape has changed.
  void OnCursorShapeChanged(scoped_ptr<MouseCursorShape> cursor_shape);

 private:
  // Points to the delegate passed to VideoFrameCapturer::Start().
  VideoFrameCapturer::Delegate* delegate_;

  // Wraps the IPC channel to the desktop session agent.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  DISALLOW_COPY_AND_ASSIGN(IpcVideoFrameCapturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
