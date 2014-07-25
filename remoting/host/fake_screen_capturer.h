// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_SCREEN_CAPTURER_H_
#define REMOTING_HOST_FAKE_SCREEN_CAPTURER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace remoting {

// A FakeScreenCapturer generates artificial image for testing purpose.
//
// FakeScreenCapturer is double-buffered as required by ScreenCapturer.
class FakeScreenCapturer : public webrtc::ScreenCapturer {
 public:
  // By default FakeScreenCapturer generates frames of size kWidth x kHeight,
  // but custom frame generator set using set_frame_generator() may generate
  // frames of different size.
  static const int kWidth = 800;
  static const int kHeight = 600;

  typedef base::Callback<scoped_ptr<webrtc::DesktopFrame>(
      webrtc::ScreenCapturer::Callback* callback)> FrameGenerator;

  FakeScreenCapturer();
  virtual ~FakeScreenCapturer();

  void set_frame_generator(const FrameGenerator& frame_generator);

  // webrtc::DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const webrtc::DesktopRegion& rect) OVERRIDE;

  // webrtc::ScreenCapturer interface.
  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE;
  virtual bool GetScreenList(ScreenList* screens) OVERRIDE;
  virtual bool SelectScreen(webrtc::ScreenId id) OVERRIDE;

 private:
  FrameGenerator frame_generator_;

  Callback* callback_;
  MouseShapeObserver* mouse_shape_observer_;

  DISALLOW_COPY_AND_ASSIGN(FakeScreenCapturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_SCREEN_CAPTURER_H_
