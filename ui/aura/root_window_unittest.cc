// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/env.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/events/event.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/base/hit_test.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

#if defined(OS_WIN)
// Windows headers define macros for these function names which screw with us.
#if defined(CreateWindow)
#undef CreateWindow
#endif
#endif

namespace aura {
namespace {

// A delegate that always returns a non-client component for hit tests.
class NonClientDelegate : public test::TestWindowDelegate {
 public:
  NonClientDelegate()
      : non_client_count_(0),
        mouse_event_count_(0),
        mouse_event_flags_(0x0) {
  }
  virtual ~NonClientDelegate() {}

  int non_client_count() const { return non_client_count_; }
  gfx::Point non_client_location() const { return non_client_location_; }
  int mouse_event_count() const { return mouse_event_count_; }
  gfx::Point mouse_event_location() const { return mouse_event_location_; }
  int mouse_event_flags() const { return mouse_event_flags_; }

  virtual int GetNonClientComponent(const gfx::Point& location) const OVERRIDE {
    NonClientDelegate* self = const_cast<NonClientDelegate*>(this);
    self->non_client_count_++;
    self->non_client_location_ = location;
    return HTTOPLEFT;
  }
  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    mouse_event_count_++;
    mouse_event_location_ = event->location();
    mouse_event_flags_ = event->flags();
    return ui::ER_HANDLED;
  }

 private:
  int non_client_count_;
  gfx::Point non_client_location_;
  int mouse_event_count_;
  gfx::Point mouse_event_location_;
  int mouse_event_flags_;

  DISALLOW_COPY_AND_ASSIGN(NonClientDelegate);
};

// A simple EventFilter that keeps track of the number of key events that it's
// seen.
class EventCountFilter : public EventFilter {
 public:
  EventCountFilter()
      : num_key_events_(0),
        num_mouse_events_(0),
        num_touch_events_(0) {
  }
  virtual ~EventCountFilter() {}

  int num_key_events() const { return num_key_events_; }
  int num_mouse_events() const { return num_mouse_events_; }
  int num_touch_events() const { return num_touch_events_; }

  void Reset() {
    num_key_events_ = 0;
    num_mouse_events_ = 0;
  }

  // EventFilter overrides:
  virtual bool PreHandleKeyEvent(Window* target, ui::KeyEvent* event) OVERRIDE {
    num_key_events_++;
    return true;
  }
  virtual bool PreHandleMouseEvent(Window* target,
                                   ui::MouseEvent* event) OVERRIDE {
    num_mouse_events_++;
    return true;
  }
  virtual ui::TouchStatus PreHandleTouchEvent(
      Window* target, ui::TouchEvent* event) OVERRIDE {
    num_touch_events_++;
    return ui::TOUCH_STATUS_UNKNOWN;
  }
  virtual ui::EventResult PreHandleGestureEvent(
      Window* target, ui::GestureEvent* event) OVERRIDE {
    return ui::ER_UNHANDLED;
  }

 private:
  // How many key events have been received?
  int num_key_events_;

  // How many mouse events have been received?
  int num_mouse_events_;

  int num_touch_events_;

  DISALLOW_COPY_AND_ASSIGN(EventCountFilter);
};

Window* CreateWindow(int id, Window* parent, WindowDelegate* delegate) {
  Window* window = new Window(
      delegate ? delegate :
      test::TestWindowDelegate::CreateSelfDestroyingDelegate());
  window->set_id(id);
  window->Init(ui::LAYER_TEXTURED);
  window->SetParent(parent);
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  return window;
}

}  // namespace

typedef test::AuraTestBase RootWindowTest;

TEST_F(RootWindowTest, OnHostMouseEvent) {
  // Create two non-overlapping windows so we don't have to worry about which
  // is on top.
  scoped_ptr<NonClientDelegate> delegate1(new NonClientDelegate());
  scoped_ptr<NonClientDelegate> delegate2(new NonClientDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  gfx::Rect bounds1(100, 200, kWindowWidth, kWindowHeight);
  gfx::Rect bounds2(300, 400, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window1(CreateTestWindowWithDelegate(
      delegate1.get(), -1234, bounds1, NULL));
  scoped_ptr<aura::Window> window2(CreateTestWindowWithDelegate(
      delegate2.get(), -5678, bounds2, NULL));

  // Send a mouse event to window1.
  gfx::Point point(101, 201);
  ui::MouseEvent event1(
      ui::ET_MOUSE_PRESSED, point, point, ui::EF_LEFT_MOUSE_BUTTON);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(&event1);

  // Event was tested for non-client area for the target window.
  EXPECT_EQ(1, delegate1->non_client_count());
  EXPECT_EQ(0, delegate2->non_client_count());
  // The non-client component test was in local coordinates.
  EXPECT_EQ(gfx::Point(1, 1), delegate1->non_client_location());
  // Mouse event was received by target window.
  EXPECT_EQ(1, delegate1->mouse_event_count());
  EXPECT_EQ(0, delegate2->mouse_event_count());
  // Event was in local coordinates.
  EXPECT_EQ(gfx::Point(1, 1), delegate1->mouse_event_location());
  // Non-client flag was set.
  EXPECT_TRUE(delegate1->mouse_event_flags() & ui::EF_IS_NON_CLIENT);
}

#if defined(OS_WIN)
// Temporarily disabled for windows. See crbug.com/112222.
TEST_F(RootWindowTest, DISABLED_HideCursor) {
#else
TEST_F(RootWindowTest, HideCursor) {
#endif  // defined(OS_WIN)
  scoped_ptr<NonClientDelegate> delegate(new NonClientDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));
  aura::Window* window_ptr = &*window;

  root_window()->ShowCursor(true);
  // Send a mouse event to window.
  gfx::Point point(101, 201);
  gfx::Point local_point;
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, point, point, 0);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(&event);

  // Location was in window.
  local_point = delegate->mouse_event_location();
  aura::Window::ConvertPointToTarget(window_ptr, root_window(), &local_point);
  EXPECT_TRUE(window->bounds().Contains(local_point));

  // Location is now out of window.
  root_window()->ShowCursor(false);
  RunAllPendingInMessageLoop();
  local_point = delegate->mouse_event_location();
  aura::Window::ConvertPointToTarget(window_ptr, root_window(), &local_point);
  EXPECT_FALSE(window->bounds().Contains(local_point));

  // Location is back in window.
  root_window()->ShowCursor(true);
  RunAllPendingInMessageLoop();
  local_point = delegate->mouse_event_location();
  aura::Window::ConvertPointToTarget(window_ptr, root_window(), &local_point);
  EXPECT_TRUE(window->bounds().Contains(local_point));
}

// Check that we correctly track the state of the mouse buttons in response to
// button press and release events.
TEST_F(RootWindowTest, MouseButtonState) {
  EXPECT_FALSE(Env::GetInstance()->is_mouse_button_down());

  gfx::Point location;
  scoped_ptr<ui::MouseEvent> event;

  // Press the left button.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_PRESSED,
      location,
      location,
      ui::EF_LEFT_MOUSE_BUTTON));
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(event.get());
  EXPECT_TRUE(Env::GetInstance()->is_mouse_button_down());

  // Additionally press the right.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_PRESSED,
      location,
      location,
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON));
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(event.get());
  EXPECT_TRUE(Env::GetInstance()->is_mouse_button_down());

  // Release the left button.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_RELEASED,
      location,
      location,
      ui::EF_RIGHT_MOUSE_BUTTON));
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(event.get());
  EXPECT_TRUE(Env::GetInstance()->is_mouse_button_down());

  // Release the right button.  We should ignore the Shift-is-down flag.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_RELEASED,
      location,
      location,
      ui::EF_SHIFT_DOWN));
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(event.get());
  EXPECT_FALSE(Env::GetInstance()->is_mouse_button_down());

  // Press the middle button.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_PRESSED,
      location,
      location,
      ui::EF_MIDDLE_MOUSE_BUTTON));
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(event.get());
  EXPECT_TRUE(Env::GetInstance()->is_mouse_button_down());
}

TEST_F(RootWindowTest, TranslatedEvent) {
  scoped_ptr<Window> w1(test::CreateTestWindowWithDelegate(NULL, 1,
      gfx::Rect(50, 50, 100, 100), NULL));

  gfx::Point origin(100, 100);
  ui::MouseEvent root(ui::ET_MOUSE_PRESSED, origin, origin, 0);

  EXPECT_EQ("100,100", root.location().ToString());
  EXPECT_EQ("100,100", root.root_location().ToString());

  ui::MouseEvent translated_event(
      root, static_cast<Window*>(root_window()), w1.get(),
      ui::ET_MOUSE_ENTERED, root.flags());
  EXPECT_EQ("50,50", translated_event.location().ToString());
  EXPECT_EQ("100,100", translated_event.root_location().ToString());
}

namespace {

class TestEventClient : public client::EventClient {
 public:
  static const int kNonLockWindowId = 100;
  static const int kLockWindowId = 200;

  explicit TestEventClient(RootWindow* root_window)
      : root_window_(root_window),
        lock_(false) {
    client::SetEventClient(root_window_, this);
    Window* lock_window =
        test::CreateTestWindowWithBounds(root_window_->bounds(), root_window_);
    lock_window->set_id(kLockWindowId);
    Window* non_lock_window =
        test::CreateTestWindowWithBounds(root_window_->bounds(), root_window_);
    non_lock_window->set_id(kNonLockWindowId);
  }
  virtual ~TestEventClient() {
    client::SetEventClient(root_window_, NULL);
  }

  // Starts/stops locking. Locking prevents windows other than those inside
  // the lock container from receiving events, getting focus etc.
  void Lock() {
    lock_ = true;
  }
  void Unlock() {
    lock_ = false;
  }

  Window* GetLockWindow() {
    return const_cast<Window*>(
        static_cast<const TestEventClient*>(this)->GetLockWindow());
  }
  const Window* GetLockWindow() const {
    return root_window_->GetChildById(kLockWindowId);
  }
  Window* GetNonLockWindow() {
    return root_window_->GetChildById(kNonLockWindowId);
  }

 private:
  // Overridden from client::EventClient:
  virtual bool CanProcessEventsWithinSubtree(
      const Window* window) const OVERRIDE {
    return lock_ ? GetLockWindow()->Contains(window) : true;
  }

  RootWindow* root_window_;
  bool lock_;

  DISALLOW_COPY_AND_ASSIGN(TestEventClient);
};

}  // namespace

TEST_F(RootWindowTest, CanProcessEventsWithinSubtree) {
  TestEventClient client(root_window());
  test::TestWindowDelegate d;

  EventCountFilter* nonlock_ef = new EventCountFilter;
  EventCountFilter* lock_ef = new EventCountFilter;
  client.GetNonLockWindow()->SetEventFilter(nonlock_ef);
  client.GetLockWindow()->SetEventFilter(lock_ef);

  Window* w1 = test::CreateTestWindowWithBounds(gfx::Rect(10, 10, 20, 20),
                                                client.GetNonLockWindow());
  w1->set_id(1);
  Window* w2 = test::CreateTestWindowWithBounds(gfx::Rect(30, 30, 20, 20),
                                                client.GetNonLockWindow());
  w2->set_id(2);
  scoped_ptr<Window> w3(
      test::CreateTestWindowWithDelegate(&d, 3, gfx::Rect(20, 20, 20, 20),
                                         client.GetLockWindow()));

  w1->Focus();
  EXPECT_TRUE(w1->GetFocusManager()->IsFocusedWindow(w1));

  client.Lock();

  // Since we're locked, the attempt to focus w2 will be ignored.
  w2->Focus();
  EXPECT_TRUE(w1->GetFocusManager()->IsFocusedWindow(w1));
  EXPECT_FALSE(w1->GetFocusManager()->IsFocusedWindow(w2));

  {
    // Attempting to send a key event to w1 (not in the lock container) should
    // cause focus to be reset.
    test::EventGenerator generator(root_window());
    generator.PressKey(ui::VKEY_SPACE, 0);
    EXPECT_EQ(NULL, w1->GetFocusManager()->GetFocusedWindow());
  }

  {
    // Events sent to a window not in the lock container will not be processed.
    // i.e. never sent to the non-lock container's event filter.
    test::EventGenerator generator(root_window(), w1);
    generator.PressLeftButton();
    EXPECT_EQ(0, nonlock_ef->num_mouse_events());

    // Events sent to a window in the lock container will be processed.
    test::EventGenerator generator3(root_window(), w3.get());
    generator3.PressLeftButton();
    EXPECT_EQ(1, lock_ef->num_mouse_events());
  }

  // Prevent w3 from being deleted by the hierarchy since its delegate is owned
  // by this scope.
  w3->parent()->RemoveChild(w3.get());
}

TEST_F(RootWindowTest, IgnoreUnknownKeys) {
  EventCountFilter* filter = new EventCountFilter;
  root_window()->SetEventFilter(filter);  // passes ownership

  ui::KeyEvent unknown_event(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, 0);
  EXPECT_FALSE(root_window()->AsRootWindowHostDelegate()->OnHostKeyEvent(
      &unknown_event));
  EXPECT_EQ(0, filter->num_key_events());

  ui::KeyEvent known_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  EXPECT_TRUE(root_window()->AsRootWindowHostDelegate()->OnHostKeyEvent(
      &known_event));
  EXPECT_EQ(1, filter->num_key_events());
}

// Tests that touch-events that are beyond the bounds of the root-window do get
// propagated to the event filters correctly with the root as the target.
TEST_F(RootWindowTest, TouchEventsOutsideBounds) {
  EventCountFilter* filter = new EventCountFilter;
  root_window()->SetEventFilter(filter);  // passes ownership

  gfx::Point position = root_window()->bounds().origin();
  position.Offset(-10, -10);
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, position, 0, base::TimeDelta());
  root_window()->AsRootWindowHostDelegate()->OnHostTouchEvent(&press);
  EXPECT_EQ(1, filter->num_touch_events());

  position = root_window()->bounds().origin();
  position.Offset(root_window()->bounds().width() + 10,
                  root_window()->bounds().height() + 10);
  ui::TouchEvent release(ui::ET_TOUCH_RELEASED, position, 0, base::TimeDelta());
  root_window()->AsRootWindowHostDelegate()->OnHostTouchEvent(&release);
  EXPECT_EQ(2, filter->num_touch_events());
}

namespace {

// FilterFilter that tracks the types of events it's seen.
class EventFilterRecorder : public EventFilter {
 public:
  typedef std::vector<ui::EventType> Events;

  EventFilterRecorder() {}

  Events& events() { return events_; }

  // EventFilter overrides:
  virtual bool PreHandleKeyEvent(Window* target, ui::KeyEvent* event) OVERRIDE {
    events_.push_back(event->type());
    return true;
  }
  virtual bool PreHandleMouseEvent(Window* target,
                                   ui::MouseEvent* event) OVERRIDE {
    events_.push_back(event->type());
    return true;
  }
  virtual ui::TouchStatus PreHandleTouchEvent(
      Window* target,
      ui::TouchEvent* event) OVERRIDE {
    events_.push_back(event->type());
    return ui::TOUCH_STATUS_UNKNOWN;
  }
  virtual ui::EventResult PreHandleGestureEvent(
      Window* target,
      ui::GestureEvent* event) OVERRIDE {
    events_.push_back(event->type());
    return ui::ER_UNHANDLED;
  }

 private:
  Events events_;

  DISALLOW_COPY_AND_ASSIGN(EventFilterRecorder);
};

// Converts an EventType to a string.
std::string EventTypeToString(ui::EventType type) {
  switch (type) {
    case ui::ET_TOUCH_RELEASED:
      return "TOUCH_RELEASED";

    case ui::ET_TOUCH_PRESSED:
      return "TOUCH_PRESSED";

    case ui::ET_TOUCH_MOVED:
      return "TOUCH_MOVED";

    case ui::ET_MOUSE_PRESSED:
      return "MOUSE_PRESSED";

    case ui::ET_MOUSE_DRAGGED:
      return "MOUSE_DRAGGED";

    case ui::ET_MOUSE_RELEASED:
      return "MOUSE_RELEASED";

    case ui::ET_MOUSE_MOVED:
      return "MOUSE_MOVED";

    case ui::ET_MOUSE_ENTERED:
      return "MOUSE_ENTERED";

    case ui::ET_MOUSE_EXITED:
      return "MOUSE_EXITED";

    case ui::ET_GESTURE_SCROLL_BEGIN:
      return "GESTURE_SCROLL_BEGIN";

    case ui::ET_GESTURE_SCROLL_END:
      return "GESTURE_SCROLL_END";

    case ui::ET_GESTURE_SCROLL_UPDATE:
      return "GESTURE_SCROLL_UPDATE";

    case ui::ET_GESTURE_TAP:
      return "GESTURE_TAP";

    case ui::ET_GESTURE_TAP_DOWN:
      return "GESTURE_TAP_DOWN";

    case ui::ET_GESTURE_BEGIN:
      return "GESTURE_BEGIN";

    case ui::ET_GESTURE_END:
      return "GESTURE_END";

    case ui::ET_GESTURE_DOUBLE_TAP:
      return "GESTURE_DOUBLE_TAP";

    default:
      break;
  }
  return "";
}

std::string EventTypesToString(const EventFilterRecorder::Events& events) {
  std::string result;
  for (size_t i = 0; i < events.size(); ++i) {
    if (i != 0)
      result += " ";
    result += EventTypeToString(events[i]);
  }
  return result;
}

}  // namespace

TEST_F(RootWindowTest, HoldMouseMove) {
  EventFilterRecorder* filter = new EventFilterRecorder;
  root_window()->SetEventFilter(filter);  // passes ownership

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), NULL));

  ui::MouseEvent mouse_move_event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                                  gfx::Point(0, 0), 0);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_move_event);
  // Discard MOUSE_ENTER.
  filter->events().clear();

  root_window()->HoldMouseMoves();

  // Check that we don't immediately dispatch the MOUSE_DRAGGED event.
  ui::MouseEvent mouse_dragged_event(ui::ET_MOUSE_DRAGGED, gfx::Point(0, 0),
                                     gfx::Point(0, 0), 0);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event);
  EXPECT_TRUE(filter->events().empty());

  // Check that we do dispatch the held MOUSE_DRAGGED event before another type
  // of event.
  ui::MouseEvent mouse_pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                                     gfx::Point(0, 0), 0);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(filter->events()));
  filter->events().clear();

  // Check that we coalesce held MOUSE_DRAGGED events.
  ui::MouseEvent mouse_dragged_event2(ui::ET_MOUSE_DRAGGED, gfx::Point(1, 1),
                                      gfx::Point(1, 1), 0);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event2);
  EXPECT_TRUE(filter->events().empty());
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(filter->events()));
  filter->events().clear();

  // Check that on ReleaseMouseMoves, held events are not dispatched
  // immediately, but posted instead.
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event);
  root_window()->ReleaseMouseMoves();
  EXPECT_TRUE(filter->events().empty());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("MOUSE_DRAGGED", EventTypesToString(filter->events()));
  filter->events().clear();

  // However if another message comes in before the dispatch,
  // the Check that on ReleaseMouseMoves, held events are not dispatched
  // immediately, but posted instead.
  root_window()->HoldMouseMoves();
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event);
  root_window()->ReleaseMouseMoves();
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(filter->events()));
  filter->events().clear();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(filter->events().empty());

  // Check that if the other message is another MOUSE_DRAGGED, we still coalesce
  // them.
  root_window()->HoldMouseMoves();
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event);
  root_window()->ReleaseMouseMoves();
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event2);
  EXPECT_EQ("MOUSE_DRAGGED", EventTypesToString(filter->events()));
  filter->events().clear();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(filter->events().empty());
}

class DeletingEventFilter : public EventFilter {
 public:
  DeletingEventFilter()
      : delete_during_pre_handle_(false) {}
  virtual ~DeletingEventFilter() {}

  void Reset(bool delete_during_pre_handle) {
    delete_during_pre_handle_ = delete_during_pre_handle;
  }

 private:
  // Overridden from EventFilter:
  virtual bool PreHandleKeyEvent(Window* target,
                                 ui::KeyEvent* event) OVERRIDE {
    if (delete_during_pre_handle_)
      delete target;
    return false;
  }

  virtual bool PreHandleMouseEvent(Window* target,
                                   ui::MouseEvent* event) OVERRIDE {
    if (delete_during_pre_handle_)
      delete target;
    return false;
  }

  bool delete_during_pre_handle_;

  DISALLOW_COPY_AND_ASSIGN(DeletingEventFilter);
};

class DeletingWindowDelegate : public test::TestWindowDelegate {
 public:
  DeletingWindowDelegate()
      : window_(NULL),
        delete_during_handle_(false),
        got_event_(false) {}
  virtual ~DeletingWindowDelegate() {}

  void Reset(Window* window, bool delete_during_handle) {
    window_ = window;
    delete_during_handle_ = delete_during_handle;
    got_event_ = false;
  }
  bool got_event() const { return got_event_; }

 private:
  // Overridden from WindowDelegate:
  virtual ui::EventResult OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    if (delete_during_handle_)
      delete window_;
    got_event_ = true;
    return ui::ER_UNHANDLED;
  }

  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (delete_during_handle_)
      delete window_;
    got_event_ = true;
    return ui::ER_UNHANDLED;
  }

  Window* window_;
  bool delete_during_handle_;
  bool got_event_;

  DISALLOW_COPY_AND_ASSIGN(DeletingWindowDelegate);
};

TEST_F(RootWindowTest, DeleteWindowDuringDispatch) {
  // Verifies that we can delete a window during each phase of event handling.
  // Deleting the window should not cause a crash, only prevent further
  // processing from occurring.
  scoped_ptr<Window> w1(CreateWindow(1, root_window(), NULL));
  DeletingWindowDelegate d11;
  Window* w11 = CreateWindow(11, w1.get(), &d11);
  WindowTracker tracker;
  DeletingEventFilter* w1_filter = new DeletingEventFilter;
  w1->SetEventFilter(w1_filter);
  w1->GetFocusManager()->SetFocusedWindow(w11, NULL);

  test::EventGenerator generator(root_window(), w11);

  // First up, no one deletes anything.
  tracker.Add(w11);
  d11.Reset(w11, false);

  generator.PressLeftButton();
  EXPECT_TRUE(tracker.Contains(w11));
  EXPECT_TRUE(d11.got_event());
  generator.ReleaseLeftButton();

  // Delegate deletes w11. This will prevent the post-handle step from applying.
  w1_filter->Reset(false);
  d11.Reset(w11, true);
  generator.PressKey(ui::VKEY_A, 0);
  EXPECT_FALSE(tracker.Contains(w11));
  EXPECT_TRUE(d11.got_event());

  // Pre-handle step deletes w11. This will prevent the delegate and the post-
  // handle steps from applying.
  w11 = CreateWindow(11, w1.get(), &d11);
  w1_filter->Reset(true);
  d11.Reset(w11, false);
  generator.PressLeftButton();
  EXPECT_FALSE(tracker.Contains(w11));
  EXPECT_FALSE(d11.got_event());
}

namespace {

// A window delegate that detaches the parent of the target's parent window when
// it receives a tap event.
class DetachesParentOnTapDelegate : public test::TestWindowDelegate {
 public:
  DetachesParentOnTapDelegate() {}
  virtual ~DetachesParentOnTapDelegate() {}

 private:
  virtual ui::EventResult OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN)
      return ui::ER_HANDLED;

    if (event->type() == ui::ET_GESTURE_TAP) {
      Window* parent = static_cast<Window*>(event->target())->parent();
      parent->parent()->RemoveChild(parent);
      return ui::ER_HANDLED;
    }
    return ui::ER_UNHANDLED;
  }

  DISALLOW_COPY_AND_ASSIGN(DetachesParentOnTapDelegate);
};

}  // namespace

// Tests that the gesture recognizer is reset for all child windows when a
// window hides. No expectations, just checks that the test does not crash.
TEST_F(RootWindowTest, GestureRecognizerResetsTargetWhenParentHides) {
  scoped_ptr<Window> w1(CreateWindow(1, root_window(), NULL));
  DetachesParentOnTapDelegate delegate;
  scoped_ptr<Window> parent(CreateWindow(22, w1.get(), NULL));
  Window* child = CreateWindow(11, parent.get(), &delegate);
  test::EventGenerator generator(root_window(), child);
  generator.GestureTapAt(gfx::Point(40, 40));
}

}  // namespace aura
