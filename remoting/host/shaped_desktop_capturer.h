// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SHAPED_DESKTOP_CAPTURER_H_
#define REMOTING_HOST_SHAPED_DESKTOP_CAPTURER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {

class DesktopShapeTracker;

// Screen capturer that also captures desktop shape.
class ShapedDesktopCapturer : public webrtc::DesktopCapturer,
                              public webrtc::DesktopCapturer::Callback {
 public:
  ShapedDesktopCapturer(scoped_ptr<webrtc::DesktopCapturer> screen_capturer,
                        scoped_ptr<DesktopShapeTracker> shape_tracker);
  ~ShapedDesktopCapturer() override;

  // webrtc::DesktopCapturer interface.
  void Start(webrtc::DesktopCapturer::Callback* callback) override;
  void SetSharedMemoryFactory(rtc::scoped_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void Capture(const webrtc::DesktopRegion& region) override;

 private:
  // webrtc::DesktopCapturer::Callback interface.
  void OnCaptureCompleted(webrtc::DesktopFrame* frame) override;

  scoped_ptr<webrtc::DesktopCapturer> desktop_capturer_;
  scoped_ptr<DesktopShapeTracker> shape_tracker_;
  webrtc::DesktopCapturer::Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(ShapedDesktopCapturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SHAPED_DESKTOP_CAPTURER_H_
