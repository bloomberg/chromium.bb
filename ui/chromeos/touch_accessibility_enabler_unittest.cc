// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/touch_accessibility_enabler.h"

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

namespace {

class MockTouchAccessibilityEnablerDelegate
    : public ui::TouchAccessibilityEnablerDelegate {
 public:
  MockTouchAccessibilityEnablerDelegate() {}
  ~MockTouchAccessibilityEnablerDelegate() override {}

  void PlaySpokenFeedbackToggleCountdown(int tick_count) override {
    ++feedback_progress_sound_count_;
  }
  void ToggleSpokenFeedback() override { toggle_spoken_feedback_ = true; }

  size_t feedback_progress_sound_count() const {
    return feedback_progress_sound_count_;
  }
  bool toggle_spoken_feedback() const { return toggle_spoken_feedback_; }

 private:
  size_t feedback_progress_sound_count_ = 0;
  bool toggle_spoken_feedback_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockTouchAccessibilityEnablerDelegate);
};

class TouchAccessibilityEnablerTest : public aura::test::AuraTestBase {
 public:
  TouchAccessibilityEnablerTest() {}
  ~TouchAccessibilityEnablerTest() override {}

  void SetUp() override {
    aura::test::AuraTestBase::SetUp();

    generator_.reset(new test::EventGenerator(root_window()));

    simulated_clock_ = new base::SimpleTestTickClock();
    // Tests fail if time is ever 0.
    simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));
    // ui takes ownership of the tick clock.
    ui::SetEventTickClockForTesting(
        std::unique_ptr<base::TickClock>(simulated_clock_));

    enabler_.reset(new TouchAccessibilityEnabler(root_window(), &delegate_));
  }

  void TearDown() override {
    enabler_.reset(nullptr);
    ui::SetEventTickClockForTesting(nullptr);
    aura::test::AuraTestBase::TearDown();
  }

 protected:
  base::TimeTicks Now() {
    // This is the same as what EventTimeForNow() does, but here we do it
    // with our simulated clock.
    return simulated_clock_->NowTicks();
  }

  std::unique_ptr<test::EventGenerator> generator_;
  // Owned by |ui|.
  base::SimpleTestTickClock* simulated_clock_ = nullptr;
  MockTouchAccessibilityEnablerDelegate delegate_;
  std::unique_ptr<TouchAccessibilityEnabler> enabler_;
  ui::GestureDetector::Config gesture_detector_config_;

  DISALLOW_COPY_AND_ASSIGN(TouchAccessibilityEnablerTest);
};

}  // namespace

TEST_F(TouchAccessibilityEnablerTest, EntersOneFingerDownMode) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  EXPECT_FALSE(enabler_->IsInOneFingerDownForTesting());
  generator_->set_current_location(gfx::Point(11, 12));
  generator_->PressTouch();

  EXPECT_FALSE(enabler_->IsInNoFingersDownForTesting());
  EXPECT_TRUE(enabler_->IsInOneFingerDownForTesting());
}

TEST_F(TouchAccessibilityEnablerTest, EntersTwoFingersDownMode) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_location(gfx::Point(11, 12));
  generator_->PressTouchId(1);

  generator_->set_current_location(gfx::Point(22, 34));
  generator_->PressTouchId(2);

  EXPECT_TRUE(enabler_->IsInTwoFingersDownForTesting());
}

TEST_F(TouchAccessibilityEnablerTest, PlaysProgressSound) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_location(gfx::Point(11, 12));
  generator_->PressTouchId(1);

  generator_->set_current_location(gfx::Point(22, 34));
  generator_->PressTouchId(2);

  EXPECT_TRUE(enabler_->IsInTwoFingersDownForTesting());
  EXPECT_EQ(0U, delegate_.feedback_progress_sound_count());

  enabler_->TriggerOnTimerForTesting();
  EXPECT_EQ(0U, delegate_.feedback_progress_sound_count());

  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(3000));
  enabler_->TriggerOnTimerForTesting();
  EXPECT_EQ(1U, delegate_.feedback_progress_sound_count());
}

TEST_F(TouchAccessibilityEnablerTest, TogglesSpokenFeedback) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_location(gfx::Point(11, 12));
  generator_->PressTouchId(1);

  generator_->set_current_location(gfx::Point(22, 34));
  generator_->PressTouchId(2);

  EXPECT_TRUE(enabler_->IsInTwoFingersDownForTesting());
  EXPECT_FALSE(delegate_.toggle_spoken_feedback());

  enabler_->TriggerOnTimerForTesting();
  EXPECT_FALSE(delegate_.toggle_spoken_feedback());

  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(5000));
  enabler_->TriggerOnTimerForTesting();
  EXPECT_TRUE(delegate_.toggle_spoken_feedback());
}

TEST_F(TouchAccessibilityEnablerTest, ThreeFingersCancelsDetection) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_location(gfx::Point(11, 12));
  generator_->PressTouchId(1);

  generator_->set_current_location(gfx::Point(22, 34));
  generator_->PressTouchId(2);

  EXPECT_TRUE(enabler_->IsInTwoFingersDownForTesting());

  generator_->set_current_location(gfx::Point(33, 56));
  generator_->PressTouchId(3);

  EXPECT_TRUE(enabler_->IsInWaitForNoFingersForTesting());
}

TEST_F(TouchAccessibilityEnablerTest, MovingFingerPastSlopCancelsDetection) {
  EXPECT_TRUE(enabler_->IsInNoFingersDownForTesting());
  generator_->set_current_location(gfx::Point(11, 12));
  generator_->PressTouch();

  int slop = gesture_detector_config_.double_tap_slop;
  int half_slop = slop / 2;

  generator_->MoveTouch(gfx::Point(11 + half_slop, 12));
  EXPECT_TRUE(enabler_->IsInOneFingerDownForTesting());

  generator_->MoveTouch(gfx::Point(11 + slop + 1, 12));
  EXPECT_TRUE(enabler_->IsInWaitForNoFingersForTesting());
}

}  // namespace ui
