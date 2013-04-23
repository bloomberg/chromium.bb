// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
#define REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/video/capture/screen/screen_capturer.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace media {
struct MouseCursorShape;
}  // namespace media

namespace remoting {

class DesktopSessionProxy;

// Routes media::ScreenCapturer calls though the IPC channel to the desktop
// session agent running in the desktop integration process.
class IpcVideoFrameCapturer : public media::ScreenCapturer {
 public:
  explicit IpcVideoFrameCapturer(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);
  virtual ~IpcVideoFrameCapturer();

  // media::ScreenCapturer interface.
  virtual void Start(Delegate* delegate) OVERRIDE;
  virtual void CaptureFrame() OVERRIDE;

  // Called when a video frame has been captured. |capture_data| describes
  // a captured frame.
  void OnCaptureCompleted(scoped_refptr<media::ScreenCaptureData> capture_data);

  // Called when the cursor shape has changed.
  void OnCursorShapeChanged(scoped_ptr<media::MouseCursorShape> cursor_shape);

 private:
  // Points to the delegate passed to media::ScreenCapturer::Start().
  media::ScreenCapturer::Delegate* delegate_;

  // Wraps the IPC channel to the desktop session agent.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  // Used to cancel tasks pending on the capturer when it is stopped.
  base::WeakPtrFactory<IpcVideoFrameCapturer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IpcVideoFrameCapturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
