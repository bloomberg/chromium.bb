// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_frame_capturer.h"

#include "base/bind.h"
#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif  // defined(OS_MACOSX)
#include "remoting/base/capture_data.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;

namespace remoting {

MATCHER(DirtyRegionIsNonEmptyRect, "") {
  const SkRegion& dirty_region = arg->dirty_region();
  const SkIRect& dirty_region_bounds = dirty_region.getBounds();
  if (dirty_region_bounds.isEmpty()) {
    return false;
  }
  return dirty_region == SkRegion(dirty_region_bounds);
}

class VideoFrameCapturerTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    capturer_ = VideoFrameCapturer::Create();
  }

  scoped_ptr<VideoFrameCapturer> capturer_;
  MockVideoFrameCapturerDelegate delegate_;
};

TEST_F(VideoFrameCapturerTest, StartCapturer) {
  capturer_->Start(&delegate_);
  capturer_->Stop();
}

TEST_F(VideoFrameCapturerTest, Capture) {
  // Assume that Start() treats the screen as invalid initially.
  EXPECT_CALL(delegate_,
              OnCaptureCompleted(DirtyRegionIsNonEmptyRect()));
  EXPECT_CALL(delegate_, OnCursorShapeChangedPtr(_))
      .Times(AnyNumber());

  capturer_->Start(&delegate_);
  capturer_->CaptureInvalidRegion();
  capturer_->Stop();
}

}  // namespace remoting
