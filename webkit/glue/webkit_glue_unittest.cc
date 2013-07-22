// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/child/webkitplatformsupport_impl.h"

namespace {

// Derives WebKitPlatformSupportImpl for testing shared timers.
class TestWebKitPlatformSupport
    : public webkit_glue::WebKitPlatformSupportImpl {
 public:
  TestWebKitPlatformSupport() : mock_monotonically_increasing_time_(0) {
  }

  // WebKitPlatformSupportImpl implementation
  virtual base::string16 GetLocalizedString(int) OVERRIDE {
    return base::string16();
  }

  virtual base::StringPiece GetDataResource(int, ui::ScaleFactor) OVERRIDE {
    return base::StringPiece();
  }

  virtual webkit_glue::ResourceLoaderBridge* CreateResourceLoader(
      const webkit_glue::ResourceLoaderBridge::RequestInfo&) OVERRIDE {
    return NULL;
  }

  virtual webkit_glue::WebSocketStreamHandleBridge* CreateWebSocketBridge(
      WebKit::WebSocketStreamHandle*,
      webkit_glue::WebSocketStreamHandleDelegate*) OVERRIDE {
    return NULL;
  }

  // Returns mock time when enabled.
  virtual double monotonicallyIncreasingTime() OVERRIDE {
    if (mock_monotonically_increasing_time_ > 0.0)
      return mock_monotonically_increasing_time_;
    return webkit_glue::WebKitPlatformSupportImpl::
        monotonicallyIncreasingTime();
  }

  virtual void OnStartSharedTimer(base::TimeDelta delay) OVERRIDE {
    shared_timer_delay_ = delay;
  }

  base::TimeDelta shared_timer_delay() {
    return shared_timer_delay_;
  }

  void set_mock_monotonically_increasing_time(double mock_time) {
    mock_monotonically_increasing_time_ = mock_time;
  }

 private:
  base::TimeDelta shared_timer_delay_;
  double mock_monotonically_increasing_time_;
};

TEST(WebkitGlueTest, SuspendResumeSharedTimer) {
  base::MessageLoop message_loop;
  TestWebKitPlatformSupport platform_support;

  // Set a timer to fire as soon as possible.
  platform_support.setSharedTimerFireInterval(0);
  // Suspend timers immediately so the above timer wouldn't be fired.
  platform_support.SuspendSharedTimer();
  // The above timer would have posted a task which can be processed out of the
  // message loop.
  message_loop.RunUntilIdle();
  // Set a mock time after 1 second to simulate timers suspended for 1 second.
  double new_time = base::Time::Now().ToDoubleT() + 1;
  platform_support.set_mock_monotonically_increasing_time(new_time);
  // Resume timers so that the timer set above will be set again to fire
  // immediately.
  platform_support.ResumeSharedTimer();
  EXPECT_TRUE(base::TimeDelta() == platform_support.shared_timer_delay());
}

}  // namespace
