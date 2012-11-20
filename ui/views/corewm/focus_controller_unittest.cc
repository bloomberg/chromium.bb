// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for:
// Activation/Focus driven by:
//  - user input events
//  - api calls
//  - window hide/destruction
//  - focus changes
// GetFocusableWindow
// GetActivatableWindow
// CanFocus
// CanActivate

// Focus/Activation behavior
// e.g. focus on activation with child windows etc.

#include "ui/views/corewm/focus_controller.h"

#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace views {
namespace corewm {

// Common infrastructure shared by all FocusController test types.
class FocusControllerTestBase : public aura::test::AuraTestBase {
 protected:
  FocusControllerTestBase() {}

  // Overridden from aura::test::AuraTestBase:
  virtual void SetUp() OVERRIDE {
    aura::test::AuraTestBase::SetUp();
    focus_controller_.reset(new FocusController);
    root_window()->AddPreTargetHandler(focus_controller());
    w1_.reset(aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 1,
        gfx::Rect(0, 0, 50, 50), NULL));
    w2_.reset(aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 2,
        gfx::Rect(75, 75, 50, 50), NULL));
  }
  virtual void TearDown() OVERRIDE {
    w2_.reset();
    w1_.reset();
    root_window()->RemovePreTargetHandler(focus_controller());
    focus_controller_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  FocusController* focus_controller() { return focus_controller_.get(); }
  aura::Window* focused_window() { return focus_controller_->focused_window(); }
  int focused_window_id() {
      return focused_window() ? focused_window()->id() : -1;
  }

  void FocusWindowById(int id) {
    aura::Window* window = root_window()->GetChildById(id);
    DCHECK(window);
    FocusWindow(window);
  }

  // Different test types shift focus in different ways.
  virtual void FocusWindow(aura::Window* window) = 0;

  void BasicFocus() {
    EXPECT_EQ(NULL, focused_window());
    FocusWindowById(1);
    EXPECT_EQ(1, focused_window_id());
    FocusWindowById(2);
    EXPECT_EQ(2, focused_window_id());
  }

 private:
  scoped_ptr<FocusController> focus_controller_;
  scoped_ptr<aura::Window> w1_;
  scoped_ptr<aura::Window> w2_;

  DISALLOW_COPY_AND_ASSIGN(FocusControllerTestBase);
};

// Focus and Activation changes via aura::client::ActivationClient API.
class FocusControllerApiTest : public FocusControllerTestBase {
 public:
  FocusControllerApiTest() {}

 private:
  // Overridden from FocusControllerTestBase:
  virtual void FocusWindow(aura::Window* window) OVERRIDE {
    focus_controller()->SetFocusedWindow(window);
  }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerApiTest);
};

// Focus and Activation changes via input events.
class FocusControllerMouseEventTest : public FocusControllerTestBase {
 public:
  FocusControllerMouseEventTest() {}

 private:
  // Overridden from FocusControllerTestBase:
  virtual void FocusWindow(aura::Window* window) OVERRIDE {
    aura::test::EventGenerator generator(root_window(), window);
    generator.ClickLeftButton();
  }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerMouseEventTest);
};

// FocusControllerGestureEventTest. - focused window set via GestureEvent.

// Focus and Activation changes in response to window visibility changes.
class FocusControllerHideTest : public FocusControllerTestBase {
 public:
  FocusControllerHideTest() {}

 private:
  // Overridden from FocusControllerTestBase:
  virtual void FocusWindow(aura::Window* window) OVERRIDE {

  }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerHideTest);
};

// FocusControllerHierarchyTest. - focused window removed from hierarchy.
// FocusControllerDestructionTest. - focused window destroyed.

#define FOCUS_CONTROLLER_TEST(TESTNAME) \
    TEST_F(FocusControllerApiTest, TESTNAME) { \
      TESTNAME(); \
    } \
    TEST_F(FocusControllerMouseEventTest, TESTNAME) { \
      TESTNAME(); \
    } \
    /* TODO(beng): make this work */ \
    TEST_F(FocusControllerHideTest, DISABLED_TESTNAME) { \
      TESTNAME(); \
    }

// - Focuses a window, verifies that focus changed.
FOCUS_CONTROLLER_TEST(BasicFocus);

// - Activates a window, verifies that activation changed.
TEST_F(FocusControllerApiTest, BasicActivation) {

}

// - Focuses a window, verifies that focus events were dispatched.
TEST_F(FocusControllerApiTest, FocusEvents) {

}

// - Activates a window, verifies that activation events were dispatched.
TEST_F(FocusControllerApiTest, ActivationEvents) {

}

// - Focuses a window, re-targets event prior to -ed event being dispatched.
TEST_F(FocusControllerApiTest, RetargetFocusEvent) {

}

// - Activates a window, re-targets event prior to -ed event being dispatched.
TEST_F(FocusControllerApiTest, RetargetActivationEvent) {

}

// - Focuses a window, consumes events.
TEST_F(FocusControllerApiTest, ConsumeFocusEvents) {

}

// - Activates a window, consumes events.
TEST_F(FocusControllerApiTest, ConsumeActivationEvents) {

}

}  // namespace corewm
}  // namespace views
