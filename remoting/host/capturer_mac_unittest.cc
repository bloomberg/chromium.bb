// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capturer.h"

#include <ApplicationServices/ApplicationServices.h>

#include <ostream>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class CapturerMacTest : public testing::Test {
 protected:
  virtual void SetUp() {
    capturer_.reset(Capturer::Create());
  }

  void AddDirtyRect() {
    SkIRect rect = SkIRect::MakeXYWH(0, 0, 10, 10);
    region_.op(rect, SkRegion::kUnion_Op);
  }

  scoped_ptr<Capturer> capturer_;
  SkRegion region_;
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
  SkRegion initial_region(SkIRect::MakeXYWH(0, 0, width, height));
  EXPECT_EQ(initial_region, capture_data->dirty_region());
}

// CapturerCallback2 verifies that a rectangle explicitly marked as dirty is
// propagated correctly.
class CapturerCallback2 {
 public:
  explicit CapturerCallback2(const SkRegion& expected_dirty_region)
      : expected_dirty_region_(expected_dirty_region) { }
  void CaptureDoneCallback(scoped_refptr<CaptureData> capture_data);

 protected:
  SkRegion expected_dirty_region_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CapturerCallback2);
};

void CapturerCallback2::CaptureDoneCallback(
    scoped_refptr<CaptureData> capture_data) {
  CGDirectDisplayID mainDevice = CGMainDisplayID();
  int width = CGDisplayPixelsWide(mainDevice);
  int height = CGDisplayPixelsHigh(mainDevice);

  EXPECT_EQ(expected_dirty_region_, capture_data->dirty_region());
  EXPECT_EQ(width, capture_data->size().width());
  EXPECT_EQ(height, capture_data->size().height());
  const DataPlanes &planes = capture_data->data_planes();
  EXPECT_TRUE(planes.data[0] != NULL);
  EXPECT_TRUE(planes.data[1] == NULL);
  EXPECT_TRUE(planes.data[2] == NULL);
  // Depending on the capture method, the screen may be flipped or not, so
  // the stride may be positive or negative.
  EXPECT_EQ(static_cast<int>(sizeof(uint32_t) * width),
            abs(planes.strides[0]));
  EXPECT_EQ(0, planes.strides[1]);
  EXPECT_EQ(0, planes.strides[2]);
}

TEST_F(CapturerMacTest, Capture) {
  SCOPED_TRACE("");
  // Check that we get an initial full-screen updated.
  CapturerCallback1 callback1;
  capturer_->CaptureInvalidRegion(
      NewCallback(&callback1, &CapturerCallback1::CaptureDoneCallback));
  // Check that subsequent dirty rects are propagated correctly.
  AddDirtyRect();
  CapturerCallback2 callback2(region_);
  capturer_->InvalidateRegion(region_);
  capturer_->CaptureInvalidRegion(
      NewCallback(&callback2, &CapturerCallback2::CaptureDoneCallback));
}

}  // namespace remoting

namespace gfx {

std::ostream& operator<<(std::ostream& out, const SkRegion& region) {
  out << "SkRegion(";
  for (SkRegion::Iterator i(region); !i.done(); i.next()) {
    const SkIRect& r = i.rect();
    out << "(" << r.fLeft << ","  << r.fTop << ","
        << r.fRight  << ","  << r.fBottom << ")";
  }
  out << ")";
  return out;
}

}  // namespace gfx
