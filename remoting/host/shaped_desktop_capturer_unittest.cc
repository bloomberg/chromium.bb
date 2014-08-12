// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/shaped_desktop_capturer.h"

#include "remoting/host/desktop_shape_tracker.h"
#include "remoting/host/fake_desktop_capturer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

class FakeDesktopShapeTracker : public DesktopShapeTracker {
 public:
  FakeDesktopShapeTracker() {}
  virtual ~FakeDesktopShapeTracker() {}

  static webrtc::DesktopRegion CreateShape() {
    webrtc::DesktopRegion result;
    result.AddRect(webrtc::DesktopRect::MakeXYWH(0, 0, 5, 5));
    result.AddRect(webrtc::DesktopRect::MakeXYWH(5, 5, 5, 5));
    return result;
  }

  virtual void RefreshDesktopShape() OVERRIDE {
    shape_ = CreateShape();
  }

  virtual const webrtc::DesktopRegion& desktop_shape() OVERRIDE {
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
  virtual webrtc::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE {
    return NULL;
  }

  virtual void OnCaptureCompleted(webrtc::DesktopFrame* frame) OVERRIDE {
    last_frame_.reset(frame);
  }

  scoped_ptr<webrtc::DesktopFrame> last_frame_;
};

// Verify that captured frame have shape.
TEST_F(ShapedDesktopCapturerTest, Basic) {
  ShapedDesktopCapturer capturer(
      scoped_ptr<webrtc::DesktopCapturer>(new FakeDesktopCapturer()),
      scoped_ptr<DesktopShapeTracker>(new FakeDesktopShapeTracker()));
  capturer.Start(this);
  capturer.Capture(webrtc::DesktopRegion());
  ASSERT_TRUE(last_frame_.get());
  ASSERT_TRUE(last_frame_->shape());
  EXPECT_TRUE(
      FakeDesktopShapeTracker::CreateShape().Equals(*last_frame_->shape()));
}

}  // namespace remoting
