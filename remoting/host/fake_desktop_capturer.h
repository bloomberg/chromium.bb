// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_DESKTOP_CAPTURER_H_
#define REMOTING_HOST_FAKE_DESKTOP_CAPTURER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capture_frame_queue.h"

namespace remoting {

// A FakeDesktopCapturer generates artificial image for testing purpose.
//
// FakeDesktopCapturer is double-buffered as required by DesktopCapturer.
class FakeDesktopCapturer : public webrtc::DesktopCapturer {
 public:
  // By default FakeDesktopCapturer generates frames of size kWidth x kHeight,
  // but custom frame generator set using set_frame_generator() may generate
  // frames of different size.
  static const int kWidth = 800;
  static const int kHeight = 600;

  typedef base::Callback<scoped_ptr<webrtc::DesktopFrame>(
      webrtc::DesktopCapturer::Callback* callback)> FrameGenerator;

  FakeDesktopCapturer();
  virtual ~FakeDesktopCapturer();

  void set_frame_generator(const FrameGenerator& frame_generator);

  // webrtc::DesktopCapturer interface.
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const webrtc::DesktopRegion& rect) OVERRIDE;

 private:
  FrameGenerator frame_generator_;

  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeDesktopCapturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_DESKTOP_CAPTURER_H_
