// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif  // defined(OS_MACOSX)
#include "remoting/base/capture_data.h"
#include "remoting/host/capturer.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

bool IsOsSupported() {
#if defined(OS_MACOSX)
  // Verify that the OS is at least Snow Leopard (10.6).
  // Chromoting doesn't support 10.5 or earlier.
  return base::mac::IsOSSnowLeopardOrLater();
#else
  return true;
#endif
}

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

class CapturerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    capturer_.reset(Capturer::Create());
  }

  scoped_ptr<Capturer> capturer_;
  MockCaptureCompletedCallback capture_completed_callback_;
};

TEST_F(CapturerTest, StartCapturer) {
  if (!IsOsSupported()) {
    return;
  }

  capturer_->Start(base::Bind(&IgnoreCursorShapeChanged));
  capturer_->Stop();
}

#if defined(OS_MACOSX)
#define MAYBE_Capture DISABLED_Capture
#else
#define MAYBE_Capture Capture
#endif

TEST_F(CapturerTest, MAYBE_Capture) {
  if (!IsOsSupported()) {
    return;
  }

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
