// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ApplicationServices/ApplicationServices.h>

#include <iostream>

#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "remoting/base/types.h"
#include "remoting/host/capturer_mac.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class CapturerMacTest : public testing::Test {
 protected:
  virtual void SetUp() {
    capturer_.reset(new CapturerMac(NULL));
  }

  void AddDirtyRect() {
    rects_.insert(gfx::Rect(0, 0, 10, 10));
  }

  scoped_ptr<CapturerMac> capturer_;
  InvalidRects rects_;
};

// CapturerCallback1 verifies that the whole screen is initially dirty.
class CapturerCallback1 {
 public:
  CapturerCallback1() { }
  void CaptureDoneCallback(scoped_refptr<CaptureData> capture_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(CapturerCallback1);
};

void CapturerCallback1::CaptureDoneCallback(
    scoped_refptr<CaptureData> capture_data) {
  CGDirectDisplayID mainDevice = CGMainDisplayID();
  int width = CGDisplayPixelsWide(mainDevice);
  int height = CGDisplayPixelsHigh(mainDevice);
  InvalidRects initial_rect;
  initial_rect.insert(gfx::Rect(0, 0, width, height));
  EXPECT_EQ(initial_rect, capture_data->dirty_rects());
}

// CapturerCallback2 verifies that a rectangle explicitly marked as dirty is
// propagated correctly.
class CapturerCallback2 {
 public:
  explicit CapturerCallback2(const InvalidRects& expected_dirty_rects)
      : expected_dirty_rects_(expected_dirty_rects) { }
  void CaptureDoneCallback(scoped_refptr<CaptureData> capture_data);

 protected:
  InvalidRects expected_dirty_rects_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CapturerCallback2);
};

void CapturerCallback2::CaptureDoneCallback(
    scoped_refptr<CaptureData> capture_data) {
  CGDirectDisplayID mainDevice = CGMainDisplayID();
  int width = CGDisplayPixelsWide(mainDevice);
  int height = CGDisplayPixelsHigh(mainDevice);

  EXPECT_EQ(expected_dirty_rects_, capture_data->dirty_rects());
  EXPECT_EQ(width, capture_data->size().width());
  EXPECT_EQ(height, capture_data->size().height());
  const DataPlanes &planes = capture_data->data_planes();
  EXPECT_TRUE(planes.data[0] != NULL);
  EXPECT_TRUE(planes.data[1] == NULL);
  EXPECT_TRUE(planes.data[2] == NULL);
  EXPECT_EQ(static_cast<int>(sizeof(uint32_t) * width),
            planes.strides[0]);
  EXPECT_EQ(0, planes.strides[1]);
  EXPECT_EQ(0, planes.strides[2]);
}

TEST_F(CapturerMacTest, Capture) {
  SCOPED_TRACE("");
  // Check that we get an initial full-screen updated.
  CapturerCallback1 callback1;
  capturer_->CaptureInvalidRects(
      NewCallback(&callback1, &CapturerCallback1::CaptureDoneCallback));
  // Check that subsequent dirty rects are propagated correctly.
  AddDirtyRect();
  CapturerCallback2 callback2(rects_);
  capturer_->InvalidateRects(rects_);
  capturer_->CaptureInvalidRects(
      NewCallback(&callback2, &CapturerCallback2::CaptureDoneCallback));
}

}  // namespace remoting

namespace gfx {

std::ostream& operator<<(std::ostream& out,
                         const remoting::InvalidRects& rects) {
  for (remoting::InvalidRects::const_iterator i = rects.begin();
       i != rects.end();
       ++i) {
    out << *i << std::endl;
  }
  return out;
}

}  // namespace gfx
