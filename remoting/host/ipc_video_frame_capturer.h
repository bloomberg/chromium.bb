// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
#define REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace media {
struct MouseCursorShape;
}  // namespace media

namespace remoting {

class DesktopSessionProxy;

// Routes webrtc::ScreenCapturer calls though the IPC channel to the desktop
// session agent running in the desktop integration process.
class IpcVideoFrameCapturer : public webrtc::ScreenCapturer {
 public:
  explicit IpcVideoFrameCapturer(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);
  virtual ~IpcVideoFrameCapturer();

  // webrtc::DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const webrtc::DesktopRegion& region) OVERRIDE;

  // webrtc::ScreenCapturer interface.
  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE;

  virtual bool GetScreenList(ScreenList* screens) OVERRIDE;

  virtual bool SelectScreen(webrtc::ScreenId id) OVERRIDE;

  // Called when a video |frame| has been captured.
  void OnCaptureCompleted(scoped_ptr<webrtc::DesktopFrame> frame);

  // Called when the cursor shape has changed.
  void OnCursorShapeChanged(scoped_ptr<webrtc::MouseCursorShape> cursor_shape);

 private:
  // Points to the callback passed to webrtc::ScreenCapturer::Start().
  webrtc::ScreenCapturer::Callback* callback_;

  MouseShapeObserver* mouse_shape_observer_;

  // Wraps the IPC channel to the desktop session agent.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;

  // Set to true when a frame is being captured.
  bool capture_pending_;

  // Used to cancel tasks pending on the capturer when it is stopped.
  base::WeakPtrFactory<IpcVideoFrameCapturer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IpcVideoFrameCapturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_VIDEO_FRAME_CAPTURER_H_
