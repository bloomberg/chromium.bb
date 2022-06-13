// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/events/base_event_utils.h"

namespace content {

class BrowserAccessibilityStateImplTest : public ::testing::Test {
 public:
  BrowserAccessibilityStateImplTest() = default;
  BrowserAccessibilityStateImplTest(const BrowserAccessibilityStateImplTest&) =
      delete;
  ~BrowserAccessibilityStateImplTest() override = default;

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutoDisableAccessibility);
    ui::SetEventTickClockForTesting(&clock_);
    // Set the initial time to something non-zero.
    clock_.Advance(base::Seconds(100));
    state_ = BrowserAccessibilityStateImpl::GetInstance();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestTickClock clock_;
  raw_ptr<BrowserAccessibilityStateImpl> state_;
  BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserAccessibilityStateImplTest,
       DisableAccessibilityBasedOnUserEvents) {
  base::HistogramTester histograms;

  // Initially, accessibility should be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());

  // Enable accessibility based on usage of accessibility APIs.
  state_->OnScreenReaderDetected();
  EXPECT_TRUE(state_->IsAccessibleBrowser());

  // Send user input, wait 31 seconds, then send another user input event.
  // Don't simulate any accessibility APIs in that time.
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();
  clock_.Advance(base::Seconds(31));
  state_->OnUserInputEvent();

  // Accessibility should now be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());

  // A histogram should record that accessibility was disabled with
  // 3 input events.
  histograms.ExpectUniqueSample("Accessibility.AutoDisabled.EventCount", 3, 1);

  // A histogram should record that accessibility was enabled for
  // 31 seconds.
  histograms.ExpectUniqueTimeSample("Accessibility.AutoDisabled.EnabledTime",
                                    base::Seconds(31), 1);
}

TEST_F(BrowserAccessibilityStateImplTest,
       AccessibilityApiUsagePreventsAutoDisableAccessibility) {
  // Initially, accessibility should be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());

  // Enable accessibility based on usage of accessibility APIs.
  state_->OnScreenReaderDetected();
  EXPECT_TRUE(state_->IsAccessibleBrowser());

  // Send user input, wait 31 seconds, then send another user input event -
  // but simulate accessibility APIs in that time.
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();
  clock_.Advance(base::Seconds(31));
  state_->OnAccessibilityApiUsage();
  state_->OnUserInputEvent();

  // Accessibility should still be enabled.
  EXPECT_TRUE(state_->IsAccessibleBrowser());

  // Same test, but simulate accessibility API usage after the first
  // user input event, before the delay.
  state_->OnUserInputEvent();
  state_->OnAccessibilityApiUsage();
  clock_.Advance(base::Seconds(31));
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();

  // Accessibility should still be enabled.
  EXPECT_TRUE(state_->IsAccessibleBrowser());

  // Advance another 31 seconds and simulate another user input event;
  // now accessibility should be disabled.
  clock_.Advance(base::Seconds(31));
  state_->OnUserInputEvent();
  EXPECT_FALSE(state_->IsAccessibleBrowser());
}

TEST_F(BrowserAccessibilityStateImplTest,
       AddAccessibilityModeFlagsPreventsAutoDisableAccessibility) {
  // Initially, accessibility should be disabled.
  EXPECT_FALSE(state_->IsAccessibleBrowser());

  // Enable accessibility.
  state_->OnScreenReaderDetected();
  EXPECT_TRUE(state_->IsAccessibleBrowser());

  // Send user input, wait 31 seconds, then send another user input event -
  // but add a new accessibility mode flag.
  state_->OnUserInputEvent();
  state_->OnUserInputEvent();
  clock_.Advance(base::Seconds(31));
  state_->AddAccessibilityModeFlags(ui::kAXModeComplete);
  state_->OnUserInputEvent();

  // Accessibility should still be enabled.
  EXPECT_TRUE(state_->IsAccessibleBrowser());
}

}  // namespace content
