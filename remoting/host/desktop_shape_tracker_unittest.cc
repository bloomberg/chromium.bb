// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DesktopShapeTracker tests assume that there is at least one top-level
// window on-screen.  Currently we assume the presence of the Explorer
// task bar window.

#include "remoting/host/desktop_shape_tracker.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

// Verify that the desktop shape tracker returns a non-empty region.
TEST(DesktopShapeTrackerTest, Basic) {
  scoped_ptr<DesktopShapeTracker> shape_tracker = DesktopShapeTracker::Create(
      webrtc::DesktopCaptureOptions::CreateDefault());

  // Shape tracker is not supported on all platforms yet.
#if defined(OS_WIN)
  shape_tracker->RefreshDesktopShape();
  EXPECT_FALSE(shape_tracker->desktop_shape().is_empty());
#else
  EXPECT_FALSE(shape_tracker);
#endif
}

}  // namespace remoting
