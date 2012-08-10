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

namespace remoting {

namespace {

void IgnoreCursorShapeChanged(scoped_ptr<protocol::CursorShapeInfo> info) {
}

}  // namespace

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
    capturer_.reset(VideoFrameCapturer::Create());
  }

  scoped_ptr<VideoFrameCapturer> capturer_;
  MockCaptureCompletedCallback capture_completed_callback_;
};

TEST_F(VideoFrameCapturerTest, StartCapturer) {
  capturer_->Start(base::Bind(&IgnoreCursorShapeChanged));
  capturer_->Stop();
}

TEST_F(VideoFrameCapturerTest, Capture) {
  // Assume that Start() treats the screen as invalid initially.
  EXPECT_CALL(capture_completed_callback_,
              CaptureCompletedPtr(DirtyRegionIsNonEmptyRect()));

  capturer_->Start(base::Bind(&IgnoreCursorShapeChanged));
  capturer_->CaptureInvalidRegion(base::Bind(
      &MockCaptureCompletedCallback::CaptureCompleted,
      base::Unretained(&capture_completed_callback_)));
  capturer_->Stop();
}

}  // namespace remoting
