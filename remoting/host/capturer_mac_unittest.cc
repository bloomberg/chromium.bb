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
    rects_.insert(gfx::Rect(0, 0, 10, 10));
  }

  scoped_ptr<CapturerMac> capturer_;
  InvalidRects rects_;
};

class CapturerCallback {
 public:
  explicit CapturerCallback(const InvalidRects& rects) : rects_(rects) { }
  void CaptureDoneCallback(scoped_refptr<CaptureData> capture_data);

 protected:
  InvalidRects rects_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CapturerCallback);
};

void CapturerCallback::CaptureDoneCallback(
    scoped_refptr<CaptureData> capture_data) {
  CGDirectDisplayID mainDevice = CGMainDisplayID();
  int width = CGDisplayPixelsWide(mainDevice);
  int height = CGDisplayPixelsHigh(mainDevice);

  EXPECT_EQ(rects_, capture_data->dirty_rects());
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
  CapturerCallback capturer(rects_);
  capturer_->InvalidateRects(rects_);
  capturer_->CaptureInvalidRects(
      NewCallback(&capturer, &CapturerCallback::CaptureDoneCallback));
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
