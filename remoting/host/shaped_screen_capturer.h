// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SHAPED_SCREEN_CAPTURER_H_
#define REMOTING_HOST_SHAPED_SCREEN_CAPTURER_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace remoting {

class DesktopShapeTracker;

// Screen capturer that also captures desktop shape.
class ShapedScreenCapturer : public webrtc::ScreenCapturer,
                             public webrtc::DesktopCapturer::Callback {
 public:
  static scoped_ptr<ShapedScreenCapturer> Create(
      webrtc::DesktopCaptureOptions options);

  ShapedScreenCapturer(scoped_ptr<webrtc::ScreenCapturer> screen_capturer,
                       scoped_ptr<DesktopShapeTracker> shape_tracker);
  virtual ~ShapedScreenCapturer();

  // webrtc::ScreenCapturer interface.
  virtual void Start(webrtc::ScreenCapturer::Callback* callback) OVERRIDE;
  virtual void Capture(const webrtc::DesktopRegion& region) OVERRIDE;
  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE;
  virtual bool GetScreenList(ScreenList* screens) OVERRIDE;
  virtual bool SelectScreen(webrtc::ScreenId id) OVERRIDE;

 private:
  // webrtc::ScreenCapturer::Callback interface.
  virtual webrtc::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual void OnCaptureCompleted(webrtc::DesktopFrame* frame) OVERRIDE;

  scoped_ptr<webrtc::ScreenCapturer> screen_capturer_;
  scoped_ptr<DesktopShapeTracker> shape_tracker_;
  webrtc::ScreenCapturer::Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(ShapedScreenCapturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SHAPED_SCREEN_CAPTURER_H_
