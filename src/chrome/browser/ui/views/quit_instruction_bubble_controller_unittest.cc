// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/quit_instruction_bubble_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ui/views/quit_instruction_bubble.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/slide_animation.h"

class TestQuitInstructionBubble : public QuitInstructionBubbleBase {
 public:
  TestQuitInstructionBubble() {}
  ~TestQuitInstructionBubble() override {}

  void Show() override {}
  void Hide() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestQuitInstructionBubble);
};

class TestQuitInstructionBubbleController
    : public QuitInstructionBubbleController {
 public:
  TestQuitInstructionBubbleController(
      std::unique_ptr<QuitInstructionBubbleBase> bubble,
      std::unique_ptr<base::OneShotTimer> hide_timer,
      std::unique_ptr<gfx::SlideAnimation> animation)
      : QuitInstructionBubbleController(std::move(bubble),
                                        std::move(hide_timer)) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestQuitInstructionBubbleController);
};

class TestSlideAnimation : public gfx::SlideAnimation {
 public:
  TestSlideAnimation() : gfx::SlideAnimation(nullptr) {}
  ~TestSlideAnimation() override {}

  void Reset() override {}
  void Reset(double value) override {}
  void Show() override {}
  void Hide() override {}
  void SetSlideDuration(int duration) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSlideAnimation);
};

class QuitInstructionBubbleControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<TestQuitInstructionBubble> bubble =
        std::make_unique<TestQuitInstructionBubble>();
    auto timer = std::make_unique<base::MockOneShotTimer>();
    bubble_ = bubble.get();
    timer_ = timer.get();
    controller_.reset(new TestQuitInstructionBubbleController(
        std::move(bubble), std::move(timer),
        std::make_unique<TestSlideAnimation>()));
  }

  void TearDown() override { controller_.reset(); }

  void SendKeyEvent(ui::KeyEvent* event) { controller_->OnKeyEvent(event); }

  void SendAccelerator(bool quit, bool press, bool repeat) {
    ui::KeyboardCode key = quit ? ui::VKEY_Q : ui::VKEY_P;
    int modifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN;
    if (repeat)
      modifiers |= ui::EF_IS_REPEAT;
    ui::EventType type = press ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED;
    ui::KeyEvent event(type, key, modifiers);
    SendKeyEvent(&event);
  }

  void PressQuitAccelerator() { SendAccelerator(true, true, false); }

  void ReleaseQuitAccelerator() { SendAccelerator(true, false, false); }

  void RepeatQuitAccelerator() { SendAccelerator(true, true, true); }

  void PressOtherAccelerator() { SendAccelerator(false, true, false); }

  void ReleaseOtherAccelerator() { SendAccelerator(false, false, false); }

  std::unique_ptr<TestQuitInstructionBubbleController> controller_;

  // Owned by |controller_|.
  TestQuitInstructionBubble* bubble_;

  // Owned by |controller_|.
  base::MockOneShotTimer* timer_;
};

// Pressing the shortcut should show the bubble.
TEST_F(QuitInstructionBubbleControllerTest, PressAndHold) {
  PressQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
  ReleaseQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
}

// Repeated presses should keep showing the bubble.
TEST_F(QuitInstructionBubbleControllerTest, RepeatedPresses) {
  PressQuitAccelerator();
  RepeatQuitAccelerator();
  ReleaseQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
}

// Other keys shouldn't matter.
TEST_F(QuitInstructionBubbleControllerTest, OtherKeyPress) {
  PressQuitAccelerator();
  ReleaseQuitAccelerator();
  PressOtherAccelerator();
  ReleaseOtherAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
  PressQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
}

// The controller should not consume keyup events on the 'Q' key
// (https://crbug.com/856868).
TEST_F(QuitInstructionBubbleControllerTest, ControllerDoesNotHandleQKeyUp) {
  ui::KeyEvent press_event(ui::ET_KEY_PRESSED, ui::VKEY_Q, 0);
  SendKeyEvent(&press_event);
  EXPECT_FALSE(press_event.handled());

  ui::KeyEvent release_event(ui::ET_KEY_RELEASED, ui::VKEY_Q, 0);
  SendKeyEvent(&release_event);
  EXPECT_FALSE(release_event.handled());
}
