// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/shaped_desktop_capturer.h"

#include <stddef.h>

#include "remoting/host/desktop_shape_tracker.h"
#include "remoting/protocol/fake_desktop_capturer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

class FakeDesktopShapeTracker : public DesktopShapeTracker {
 public:
  FakeDesktopShapeTracker() {}
  ~FakeDesktopShapeTracker() override {}

  static webrtc::DesktopRegion CreateShape() {
    webrtc::DesktopRegion result;
    result.AddRect(webrtc::DesktopRect::MakeXYWH(0, 0, 5, 5));
    result.AddRect(webrtc::DesktopRect::MakeXYWH(5, 5, 5, 5));
    return result;
  }

  void RefreshDesktopShape() override { shape_ = CreateShape(); }

  const webrtc::DesktopRegion& desktop_shape() override {
    // desktop_shape() can't be called before RefreshDesktopShape().
    EXPECT_FALSE(shape_.is_empty());
    return shape_;
  }

 private:
  webrtc::DesktopRegion shape_;
};

class ShapedDesktopCapturerTest : public testing::Test,
                                 public webrtc::DesktopCapturer::Callback {
 public:
  // webrtc::DesktopCapturer::Callback interface
  void OnCaptureCompleted(webrtc::DesktopFrame* frame) override {
    last_frame_.reset(frame);
  }

  scoped_ptr<webrtc::DesktopFrame> last_frame_;
};

// Verify that captured frame have shape.
TEST_F(ShapedDesktopCapturerTest, Basic) {
  ShapedDesktopCapturer capturer(
      make_scoped_ptr(new protocol::FakeDesktopCapturer()),
      make_scoped_ptr(new FakeDesktopShapeTracker()));
  capturer.Start(this);
  capturer.Capture(webrtc::DesktopRegion());
  ASSERT_TRUE(last_frame_.get());
  ASSERT_TRUE(last_frame_->shape());
  EXPECT_TRUE(
      FakeDesktopShapeTracker::CreateShape().Equals(*last_frame_->shape()));
}

}  // namespace remoting
