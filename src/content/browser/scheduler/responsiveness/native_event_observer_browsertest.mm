// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/native_event_observer.h"

#include "base/bind_helpers.h"
#include "content/public/test/content_browser_test.h"
#include "ui/events/test/cocoa_test_event_utils.h"

#import <Carbon/Carbon.h>

namespace content {
namespace responsiveness {

namespace {

class FakeNativeEventObserver : public NativeEventObserver {
 public:
  FakeNativeEventObserver()
      : NativeEventObserver(base::DoNothing(), base::DoNothing()) {}
  ~FakeNativeEventObserver() override = default;

  void WillRunNativeEvent(const void* opaque_identifier) override {
    ASSERT_FALSE(will_run_id_);
    will_run_id_ = opaque_identifier;
  }
  void DidRunNativeEvent(const void* opaque_identifier,
                         base::TimeTicks creation_time) override {
    ASSERT_FALSE(did_run_id_);
    did_run_id_ = opaque_identifier;
    creation_time_ = creation_time;
  }

  const void* will_run_id() { return will_run_id_; }
  const void* did_run_id() { return did_run_id_; }
  base::TimeTicks creation_time() { return creation_time_; }

 private:
  const void* will_run_id_ = nullptr;
  const void* did_run_id_ = nullptr;
  base::TimeTicks creation_time_;
};

}  // namespace

class ResponsivenessNativeEventObserverBrowserTest : public ContentBrowserTest {
};

IN_PROC_BROWSER_TEST_F(ResponsivenessNativeEventObserverBrowserTest,
                       EventForwarding) {
  FakeNativeEventObserver observer;

  EXPECT_FALSE(observer.will_run_id());
  EXPECT_FALSE(observer.did_run_id());
  base::TimeTicks time_at_creation = base::TimeTicks::Now();
  NSEvent* event = cocoa_test_event_utils::KeyEventWithKeyCode(kVK_Return, '\r',
                                                               NSKeyDown, 0);
  [NSApp sendEvent:event];

  EXPECT_EQ(observer.will_run_id(), event);
  EXPECT_EQ(observer.did_run_id(), event);

  // time_at_creation should be really similar to creation_time. As a sanity
  // check, make sure they're within a second of each other.
  EXPECT_LT(
      fabs((observer.creation_time() - time_at_creation).InMilliseconds()),
      1000);
}

}  // namespace responsiveness
}  // namespace content
