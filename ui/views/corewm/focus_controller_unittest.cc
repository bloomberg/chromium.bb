// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/focus_controller.h"

#include <map>

#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/events/event_handler.h"
#include "ui/views/corewm/base_focus_rules.h"
#include "ui/views/corewm/focus_change_event.h"

namespace views {
namespace corewm {

class FocusEventsTestHandler : public ui::EventHandler,
                               public aura::WindowObserver {
 public:
  explicit FocusEventsTestHandler(aura::Window* window)
      : window_(window),
        result_(ui::ER_UNHANDLED) {
    window_->AddObserver(this);
    window_->AddPreTargetHandler(this);
  }
  virtual ~FocusEventsTestHandler() {
    RemoveObserver();
  }

  void set_result(ui::EventResult result) { result_ = result; }

  int GetCountForEventType(int event_type) {
    std::map<int, int>::const_iterator it = event_counts_.find(event_type);
    return it != event_counts_.end() ? it->second : 0;
  }

 private:
  // Overridden from ui::EventHandler:
  virtual ui::EventResult OnEvent(ui::Event* event) OVERRIDE {
    event_counts_[event->type()] += 1;
    return result_;
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
    DCHECK_EQ(window, window_);
    RemoveObserver();
  }

  void RemoveObserver() {
    if (window_) {
      window_->RemoveObserver(this);
      window_->RemovePreTargetHandler(this);
      window_ = NULL;
    }
  }

  aura::Window* window_;
  std::map<int, int> event_counts_;
  ui::EventResult result_;

  DISALLOW_COPY_AND_ASSIGN(FocusEventsTestHandler);
};

// Common infrastructure shared by all FocusController test types.
class FocusControllerTestBase : public aura::test::AuraTestBase {
 protected:
  FocusControllerTestBase() {}

  // Overridden from aura::test::AuraTestBase:
  virtual void SetUp() OVERRIDE {
    // FocusController registers itself as an Env observer so it can catch all
    // window initializations, including the root_window()'s, so we create it
    // before allowing the base setup.
    focus_controller_.reset(new FocusController(new BaseFocusRules));
    aura::test::AuraTestBase::SetUp();
    root_window()->AddPreTargetHandler(focus_controller());
    aura::client::SetActivationClient(root_window(), focus_controller());

    // Hierarchy used by all tests:
    // root_window
    //       +-- w1
    //            +-- w11
    //       +-- w2
    //            +-- w21
    //                 +-- w211
    aura::Window* w1 = aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 1,
        gfx::Rect(0, 0, 50, 50), NULL);
    aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 11,
        gfx::Rect(5, 5, 10, 10), w1);
    aura::Window* w2 = aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 2,
        gfx::Rect(75, 75, 50, 50), NULL);
    aura::Window* w21 = aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 21,
        gfx::Rect(5, 5, 10, 10), w2);
    aura::test::CreateTestWindowWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), 211,
        gfx::Rect(1, 1, 5, 5), w21);
  }
  virtual void TearDown() OVERRIDE {
    root_window()->RemovePreTargetHandler(focus_controller());
    aura::test::AuraTestBase::TearDown();
    focus_controller_.reset();
  }

  FocusController* focus_controller() { return focus_controller_.get(); }
  aura::Window* focused_window() { return focus_controller_->focused_window(); }
  int focused_window_id() {
      return focused_window() ? focused_window()->id() : -1;
  }

  void ActivateWindow(aura::Window* window) {
    aura::client::GetActivationClient(root_window())->ActivateWindow(window);
  }
  aura::Window* GetActiveWindow() {
    return aura::client::GetActivationClient(root_window())->GetActiveWindow();
  }
  int GetActiveWindowId() {
    aura::Window* active_window = GetActiveWindow();
    return active_window ? active_window->id() : -1;
  }

  void ExpectActivationEvents(FocusEventsTestHandler* handler,
                              int expected_changing_event_count,
                              int expected_changed_event_count) {
    EXPECT_EQ(expected_changing_event_count,
              handler->GetCountForEventType(
                  FocusChangeEvent::activation_changing_event_type()));
    EXPECT_EQ(expected_changed_event_count,
              handler->GetCountForEventType(
                  FocusChangeEvent::activation_changed_event_type()));
  }

  // Test functions.
  virtual void BasicFocus() = 0;
  virtual void BasicActivation() = 0;
  virtual void FocusEvents() = 0;
  virtual void ActivationEvents() = 0;

 private:
  scoped_ptr<FocusController> focus_controller_;

  DISALLOW_COPY_AND_ASSIGN(FocusControllerTestBase);
};

// Test base for tests where focus is directly set to a target window.
class FocusControllerDirectTestBase : public FocusControllerTestBase {
 protected:
  FocusControllerDirectTestBase() {}

  // Different test types shift focus in different ways.
  virtual void FocusWindowDirect(aura::Window* window) = 0;
  virtual void ActivateWindowDirect(aura::Window* window) = 0;

  void FocusWindowById(int id) {
    aura::Window* window = root_window()->GetChildById(id);
    DCHECK(window);
    FocusWindowDirect(window);
  }
  void ActivateWindowById(int id) {
    aura::Window* window = root_window()->GetChildById(id);
    DCHECK(window);
    ActivateWindowDirect(window);
  }

  // Overridden from FocusControllerTestBase:
  virtual void BasicFocus() OVERRIDE {
    EXPECT_EQ(NULL, focused_window());
    FocusWindowById(1);
    EXPECT_EQ(1, focused_window_id());
    FocusWindowById(2);
    EXPECT_EQ(2, focused_window_id());
  }
  virtual void BasicActivation() OVERRIDE {
    EXPECT_EQ(NULL, GetActiveWindow());
    ActivateWindowById(1);
    EXPECT_EQ(1, GetActiveWindowId());
    ActivateWindowById(2);
    EXPECT_EQ(2, GetActiveWindowId());
  }
  virtual void FocusEvents() OVERRIDE {
    FocusEventsTestHandler handler(root_window()->GetChildById(1));
    EXPECT_EQ(0, handler.GetCountForEventType(
        FocusChangeEvent::focus_changing_event_type()));
    EXPECT_EQ(0, handler.GetCountForEventType(
        FocusChangeEvent::focus_changed_event_type()));
    FocusWindowById(1);
    EXPECT_EQ(1, handler.GetCountForEventType(
        FocusChangeEvent::focus_changing_event_type()));
    EXPECT_EQ(1, handler.GetCountForEventType(
        FocusChangeEvent::focus_changed_event_type()));
  }

  virtual void ActivationEvents() OVERRIDE {
    ActivateWindowById(1);

    FocusEventsTestHandler handler_root(root_window());
    FocusEventsTestHandler handler_1(root_window()->GetChildById(1));
    FocusEventsTestHandler handler_2(root_window()->GetChildById(2));

    ExpectActivationEvents(&handler_root, 0, 0);
    ExpectActivationEvents(&handler_1, 0, 0);
    ExpectActivationEvents(&handler_2, 0, 0);
    ActivateWindowById(2);
    ExpectActivationEvents(&handler_root, 1, 1);
    ExpectActivationEvents(&handler_1, 1, 0);
    ExpectActivationEvents(&handler_2, 0, 1);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerDirectTestBase);
};

// Focus and Activation changes via aura::client::ActivationClient API.
class FocusControllerApiTest : public FocusControllerDirectTestBase {
 public:
  FocusControllerApiTest() {}

 private:
  // Overridden from FocusControllerTestBase:
  virtual void FocusWindowDirect(aura::Window* window) OVERRIDE {
    focus_controller()->FocusWindow(window);
  }
  virtual void ActivateWindowDirect(aura::Window* window) OVERRIDE {
    ActivateWindow(window);
  }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerApiTest);
};

// Focus and Activation changes via input events.
class FocusControllerMouseEventTest : public FocusControllerDirectTestBase {
 public:
  FocusControllerMouseEventTest() {}

 private:
  // Overridden from FocusControllerTestBase:
  virtual void FocusWindowDirect(aura::Window* window) OVERRIDE {
    aura::test::EventGenerator generator(root_window(), window);
    generator.ClickLeftButton();
  }
  virtual void ActivateWindowDirect(aura::Window* window) OVERRIDE {
    aura::test::EventGenerator generator(root_window(), window);
    generator.ClickLeftButton();
  }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerMouseEventTest);
};

class FocusControllerGestureEventTest : public FocusControllerDirectTestBase {
 public:
  FocusControllerGestureEventTest() {}

 private:
  // Overridden from FocusControllerTestBase:
  virtual void FocusWindowDirect(aura::Window* window) OVERRIDE {
    aura::test::EventGenerator generator(root_window(), window);
    generator.GestureTapAt(window->bounds().CenterPoint());
  }
  virtual void ActivateWindowDirect(aura::Window* window) OVERRIDE {
    aura::test::EventGenerator generator(root_window(), window);
    generator.GestureTapAt(window->bounds().CenterPoint());
  }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerGestureEventTest);
};

// Test base for tests where focus is implicitly set to a window as the result
// of a disposition change to the focused window or the hierarchy that contains
// it.
class FocusControllerImplicitTestBase : public FocusControllerTestBase {
 protected:
  explicit FocusControllerImplicitTestBase(bool parent) : parent_(parent) {}

  aura::Window* GetDispositionWindow(aura::Window* window) {
    return parent_ ? window->parent() : window;
  }

  // Change the disposition of |window| in such a way as it will lose focus.
  virtual void ChangeWindowDisposition(aura::Window* window) = 0;

  // Overridden from FocusControllerTestBase:
  virtual void BasicFocus() OVERRIDE {
    EXPECT_EQ(NULL, focused_window());

    aura::Window* w211 = root_window()->GetChildById(211);
    focus_controller()->FocusWindow(w211);
    EXPECT_EQ(211, focused_window_id());

    ChangeWindowDisposition(w211);
    // BasicFocusRules passes focus to the parent.
    EXPECT_EQ(parent_ ? 2 : 21, focused_window_id());
  }
  virtual void BasicActivation() OVERRIDE {
    DCHECK(!parent_) << "Activation tests don't support parent changes.";

    EXPECT_EQ(NULL, GetActiveWindow());

    aura::Window* w2 = root_window()->GetChildById(2);
    ActivateWindow(w2);
    EXPECT_EQ(2, GetActiveWindowId());

    ChangeWindowDisposition(w2);
    EXPECT_EQ(1, GetActiveWindowId());
  }
  virtual void FocusEvents() OVERRIDE {
    aura::Window* w211 = root_window()->GetChildById(211);
    focus_controller()->FocusWindow(w211);

    FocusEventsTestHandler handler(root_window()->GetChildById(211));
    EXPECT_EQ(0, handler.GetCountForEventType(
        FocusChangeEvent::focus_changing_event_type()));
    EXPECT_EQ(0, handler.GetCountForEventType(
        FocusChangeEvent::focus_changed_event_type()));
    ChangeWindowDisposition(w211);
    EXPECT_EQ(1, handler.GetCountForEventType(
        FocusChangeEvent::focus_changing_event_type()));
    EXPECT_EQ(1, handler.GetCountForEventType(
        FocusChangeEvent::focus_changed_event_type()));
  }
  virtual void ActivationEvents() OVERRIDE {
    DCHECK(!parent_) << "Activation tests don't support parent changes.";

    aura::Window* w2 = root_window()->GetChildById(2);
    ActivateWindow(w2);

    FocusEventsTestHandler handler_root(root_window());
    FocusEventsTestHandler handler_1(root_window()->GetChildById(1));
    FocusEventsTestHandler handler_2(root_window()->GetChildById(2));

    ExpectActivationEvents(&handler_root, 0, 0);
    ExpectActivationEvents(&handler_1, 0, 0);
    ExpectActivationEvents(&handler_2, 0, 0);

    ChangeWindowDisposition(w2);
    ExpectActivationEvents(&handler_root, 1, 1);
    ExpectActivationEvents(&handler_1, 0, 1);
    ExpectActivationEvents(&handler_2, 1, 0);
  }

 private:
  // When true, the disposition change occurs to the parent of the window
  // instead of to the window. This verifies that changes occurring in the
  // hierarchy that contains the window affect the window's focus.
  bool parent_;

  DISALLOW_COPY_AND_ASSIGN(FocusControllerImplicitTestBase);
};

// Focus and Activation changes in response to window visibility changes.
class FocusControllerHideTest : public FocusControllerImplicitTestBase {
 public:
  FocusControllerHideTest() : FocusControllerImplicitTestBase(false) {}

 protected:
  FocusControllerHideTest(bool parent)
      : FocusControllerImplicitTestBase(parent) {}

  // Overridden from FocusControllerImplicitTestBase:
  virtual void ChangeWindowDisposition(aura::Window* window) OVERRIDE {
    GetDispositionWindow(window)->Hide();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerHideTest);
};

// Focus and Activation changes in response to window parent visibility
// changes.
class FocusControllerParentHideTest : public FocusControllerHideTest {
 public:
  FocusControllerParentHideTest() : FocusControllerHideTest(true) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerParentHideTest);
};

// Focus and Activation changes in response to window destruction.
class FocusControllerDestructionTest : public FocusControllerImplicitTestBase {
 public:
  FocusControllerDestructionTest() : FocusControllerImplicitTestBase(false) {}

 protected:
  FocusControllerDestructionTest(bool parent)
      : FocusControllerImplicitTestBase(parent) {}

  // Overridden from FocusControllerImplicitTestBase:
  virtual void ChangeWindowDisposition(aura::Window* window) OVERRIDE {
    delete GetDispositionWindow(window);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerDestructionTest);
};

// Focus and Activation changes in response to window parent destruction.
class FocusControllerParentDestructionTest
    : public FocusControllerDestructionTest {
 public:
  FocusControllerParentDestructionTest()
      : FocusControllerDestructionTest(true) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerParentDestructionTest);
};

// Focus and Activation changes in response to window removal.
class FocusControllerRemovalTest : public FocusControllerImplicitTestBase {
 public:
  FocusControllerRemovalTest() : FocusControllerImplicitTestBase(false) {}

 protected:
  FocusControllerRemovalTest(bool parent)
      : FocusControllerImplicitTestBase(parent) {}

  // Overridden from FocusControllerImplicitTestBase:
  virtual void ChangeWindowDisposition(aura::Window* window) OVERRIDE {
    aura::Window* disposition_window = GetDispositionWindow(window);
    disposition_window->parent()->RemoveChild(disposition_window);
    window_owner_.reset(disposition_window);
  }
  virtual void TearDown() OVERRIDE {
    window_owner_.reset();
    FocusControllerImplicitTestBase::TearDown();
  }

 private:
  scoped_ptr<aura::Window> window_owner_;

  DISALLOW_COPY_AND_ASSIGN(FocusControllerRemovalTest);
};

// Focus and Activation changes in response to window parent removal.
class FocusControllerParentRemovalTest : public FocusControllerRemovalTest {
 public:
  FocusControllerParentRemovalTest() : FocusControllerRemovalTest(true) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerParentRemovalTest);
};


#define FOCUS_CONTROLLER_TEST(TESTCLASS, TESTNAME) \
    TEST_F(TESTCLASS, TESTNAME) { TESTNAME(); }

#define FOCUS_TESTS(TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerApiTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerMouseEventTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerGestureEventTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerHideTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerParentHideTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerDestructionTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerParentDestructionTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerRemovalTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerParentRemovalTest, TESTNAME)

// Activation tests do not have Parent* variants as that is not a supported
// event flow.
#define ACTIVATION_TESTS(TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerApiTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerMouseEventTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerGestureEventTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerHideTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerDestructionTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerRemovalTest, TESTNAME)

// - Focuses a window, verifies that focus changed.
FOCUS_TESTS(BasicFocus);

// - Activates a window, verifies that activation changed.
ACTIVATION_TESTS(BasicActivation);

// - Focuses a window, verifies that focus events were dispatched.
FOCUS_TESTS(FocusEvents);

// - Activates a window, verifies that activation events were dispatched.
ACTIVATION_TESTS(ActivationEvents);

// - Focuses a window, re-targets event prior to -ed event being dispatched.
TEST_F(FocusControllerApiTest, RetargetFocusEvent) {

}

// - Activates a window, re-targets event prior to -ed event being dispatched.
TEST_F(FocusControllerApiTest, RetargetActivationEvent) {

}

// TODO(beng):
// . window->window focus changes within an active window
// . focus and activation changes within a single test.

}  // namespace corewm
}  // namespace views
