// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_SCREEN_CAPTURER_H_
#define REMOTING_HOST_FAKE_SCREEN_CAPTURER_H_

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
  // FakeScreenCapturer generates a picture of size kWidth x kHeight.
  static const int kWidth = 800;
  static const int kHeight = 600;

  FakeScreenCapturer();
  virtual ~FakeScreenCapturer();

  // webrtc::DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const webrtc::DesktopRegion& rect) OVERRIDE;

  // ScreenCapturer interface.
  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE;
  virtual bool GetScreenList(ScreenList* screens) OVERRIDE;
  virtual bool SelectScreen(webrtc::ScreenId id) OVERRIDE;

 private:
  // Generates an image in the front buffer.
  void GenerateImage();

  // Called when the screen configuration is changed.
  void ScreenConfigurationChanged();

  Callback* callback_;
  MouseShapeObserver* mouse_shape_observer_;

  webrtc::DesktopSize size_;
  int bytes_per_row_;
  int box_pos_x_;
  int box_pos_y_;
  int box_speed_x_;
  int box_speed_y_;

  webrtc::ScreenCaptureFrameQueue queue_;

  DISALLOW_COPY_AND_ASSIGN(FakeScreenCapturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_SCREEN_CAPTURER_H_
