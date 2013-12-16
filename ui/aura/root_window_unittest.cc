// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window.h"

#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/test/test_event_handler.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/transform.h"

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
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    mouse_event_count_++;
    mouse_event_location_ = event->location();
    mouse_event_flags_ = event->flags();
    event->SetHandled();
  }

 private:
  int non_client_count_;
  gfx::Point non_client_location_;
  int mouse_event_count_;
  gfx::Point mouse_event_location_;
  int mouse_event_flags_;

  DISALLOW_COPY_AND_ASSIGN(NonClientDelegate);
};

// A simple event handler that consumes key events.
class ConsumeKeyHandler : public test::TestEventHandler {
 public:
  ConsumeKeyHandler() {}
  virtual ~ConsumeKeyHandler() {}

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    test::TestEventHandler::OnKeyEvent(event);
    event->StopPropagation();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConsumeKeyHandler);
};

bool IsFocusedWindow(aura::Window* window) {
  return client::GetFocusClient(window)->GetFocusedWindow() == window;
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
      delegate1.get(), -1234, bounds1, root_window()));
  scoped_ptr<aura::Window> window2(CreateTestWindowWithDelegate(
      delegate2.get(), -5678, bounds2, root_window()));

  // Send a mouse event to window1.
  gfx::Point point(101, 201);
  ui::MouseEvent event1(
      ui::ET_MOUSE_PRESSED, point, point, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(&event1);

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

TEST_F(RootWindowTest, RepostEvent) {
  // Test RepostEvent in RootWindow. It only works for Mouse Press.
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());
  gfx::Point point(10, 10);
  ui::MouseEvent event(
      ui::ET_MOUSE_PRESSED, point, point, ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher()->RepostEvent(event);
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());
}

// Check that we correctly track the state of the mouse buttons in response to
// button press and release events.
TEST_F(RootWindowTest, MouseButtonState) {
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());

  gfx::Point location;
  scoped_ptr<ui::MouseEvent> event;

  // Press the left button.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_PRESSED,
      location,
      location,
      ui::EF_LEFT_MOUSE_BUTTON));
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());

  // Additionally press the right.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_PRESSED,
      location,
      location,
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON));
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());

  // Release the left button.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_RELEASED,
      location,
      location,
      ui::EF_RIGHT_MOUSE_BUTTON));
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());

  // Release the right button.  We should ignore the Shift-is-down flag.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_RELEASED,
      location,
      location,
      ui::EF_SHIFT_DOWN));
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(event.get());
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());

  // Press the middle button.
  event.reset(new ui::MouseEvent(
      ui::ET_MOUSE_PRESSED,
      location,
      location,
      ui::EF_MIDDLE_MOUSE_BUTTON));
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(event.get());
  EXPECT_TRUE(Env::GetInstance()->IsMouseButtonDown());
}

TEST_F(RootWindowTest, TranslatedEvent) {
  scoped_ptr<Window> w1(test::CreateTestWindowWithDelegate(NULL, 1,
      gfx::Rect(50, 50, 100, 100), root_window()));

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

  explicit TestEventClient(Window* root_window)
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
    return lock_ ?
        window->Contains(GetLockWindow()) || GetLockWindow()->Contains(window) :
        true;
  }

  virtual ui::EventTarget* GetToplevelEventTarget() OVERRIDE {
    return NULL;
  }

  Window* root_window_;
  bool lock_;

  DISALLOW_COPY_AND_ASSIGN(TestEventClient);
};

}  // namespace

TEST_F(RootWindowTest, CanProcessEventsWithinSubtree) {
  TestEventClient client(root_window());
  test::TestWindowDelegate d;

  test::TestEventHandler* nonlock_ef = new test::TestEventHandler;
  test::TestEventHandler* lock_ef = new test::TestEventHandler;
  client.GetNonLockWindow()->SetEventFilter(nonlock_ef);
  client.GetLockWindow()->SetEventFilter(lock_ef);

  Window* w1 = test::CreateTestWindowWithBounds(gfx::Rect(10, 10, 20, 20),
                                                client.GetNonLockWindow());
  w1->set_id(1);
  Window* w2 = test::CreateTestWindowWithBounds(gfx::Rect(30, 30, 20, 20),
                                                client.GetNonLockWindow());
  w2->set_id(2);
  scoped_ptr<Window> w3(
      test::CreateTestWindowWithDelegate(&d, 3, gfx::Rect(30, 30, 20, 20),
                                         client.GetLockWindow()));

  w1->Focus();
  EXPECT_TRUE(IsFocusedWindow(w1));

  client.Lock();

  // Since we're locked, the attempt to focus w2 will be ignored.
  w2->Focus();
  EXPECT_TRUE(IsFocusedWindow(w1));
  EXPECT_FALSE(IsFocusedWindow(w2));

  {
    // Attempting to send a key event to w1 (not in the lock container) should
    // cause focus to be reset.
    test::EventGenerator generator(root_window());
    generator.PressKey(ui::VKEY_SPACE, 0);
    EXPECT_EQ(NULL, client::GetFocusClient(w1)->GetFocusedWindow());
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
  test::TestEventHandler* filter = new ConsumeKeyHandler;
  root_window()->SetEventFilter(filter);  // passes ownership

  ui::KeyEvent unknown_event(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, 0, false);
  EXPECT_FALSE(dispatcher()->AsRootWindowHostDelegate()->OnHostKeyEvent(
      &unknown_event));
  EXPECT_EQ(0, filter->num_key_events());

  ui::KeyEvent known_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, false);
  EXPECT_TRUE(dispatcher()->AsRootWindowHostDelegate()->OnHostKeyEvent(
      &known_event));
  EXPECT_EQ(1, filter->num_key_events());
}

TEST_F(RootWindowTest, NoDelegateWindowReceivesKeyEvents) {
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  w1->Show();
  w1->Focus();

  test::TestEventHandler handler;
  w1->AddPreTargetHandler(&handler);
  ui::KeyEvent key_press(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, false);
  EXPECT_TRUE(dispatcher()->AsRootWindowHostDelegate()->OnHostKeyEvent(
      &key_press));
  EXPECT_EQ(1, handler.num_key_events());

  w1->RemovePreTargetHandler(&handler);
}

// Tests that touch-events that are beyond the bounds of the root-window do get
// propagated to the event filters correctly with the root as the target.
TEST_F(RootWindowTest, TouchEventsOutsideBounds) {
  test::TestEventHandler* filter = new test::TestEventHandler;
  root_window()->SetEventFilter(filter);  // passes ownership

  gfx::Point position = root_window()->bounds().origin();
  position.Offset(-10, -10);
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, position, 0, base::TimeDelta());
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(&press);
  EXPECT_EQ(1, filter->num_touch_events());

  position = root_window()->bounds().origin();
  position.Offset(root_window()->bounds().width() + 10,
                  root_window()->bounds().height() + 10);
  ui::TouchEvent release(ui::ET_TOUCH_RELEASED, position, 0, base::TimeDelta());
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(&release);
  EXPECT_EQ(2, filter->num_touch_events());
}

// Tests that scroll events are dispatched correctly.
TEST_F(RootWindowTest, ScrollEventDispatch) {
  base::TimeDelta now = ui::EventTimeForNow();
  test::TestEventHandler* filter = new test::TestEventHandler;
  root_window()->SetEventFilter(filter);

  test::TestWindowDelegate delegate;
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), &delegate));
  w1->SetBounds(gfx::Rect(20, 20, 40, 40));

  // A scroll event on the root-window itself is dispatched.
  ui::ScrollEvent scroll1(ui::ET_SCROLL,
                          gfx::Point(10, 10),
                          now,
                          0,
                          0, -10,
                          0, -10,
                          2);
  dispatcher()->AsRootWindowHostDelegate()->OnHostScrollEvent(&scroll1);
  EXPECT_EQ(1, filter->num_scroll_events());

  // Scroll event on a window should be dispatched properly.
  ui::ScrollEvent scroll2(ui::ET_SCROLL,
                          gfx::Point(25, 30),
                          now,
                          0,
                          -10, 0,
                          -10, 0,
                          2);
  dispatcher()->AsRootWindowHostDelegate()->OnHostScrollEvent(&scroll2);
  EXPECT_EQ(2, filter->num_scroll_events());
}

namespace {

// FilterFilter that tracks the types of events it's seen.
class EventFilterRecorder : public ui::EventHandler {
 public:
  typedef std::vector<ui::EventType> Events;
  typedef std::vector<gfx::Point> MouseEventLocations;

  EventFilterRecorder() {}

  Events& events() { return events_; }

  MouseEventLocations& mouse_locations() { return mouse_locations_; }
  gfx::Point mouse_location(int i) const { return mouse_locations_[i]; }

Events GetAndResetEvents() {
    Events events = events_;
    events_.clear();
    return events;
  }

  // ui::EventHandler overrides:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    events_.push_back(event->type());
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    events_.push_back(event->type());
    mouse_locations_.push_back(event->location());
  }

  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE {
    events_.push_back(event->type());
  }

  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE {
    events_.push_back(event->type());
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    events_.push_back(event->type());
  }

 private:
  Events events_;
  MouseEventLocations mouse_locations_;

  DISALLOW_COPY_AND_ASSIGN(EventFilterRecorder);
};

// Converts an EventType to a string.
std::string EventTypeToString(ui::EventType type) {
  switch (type) {
    case ui::ET_TOUCH_RELEASED:
      return "TOUCH_RELEASED";

    case ui::ET_TOUCH_CANCELLED:
      return "TOUCH_CANCELLED";

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

    case ui::ET_GESTURE_PINCH_BEGIN:
      return "GESTURE_PINCH_BEGIN";

    case ui::ET_GESTURE_PINCH_END:
      return "GESTURE_PINCH_END";

    case ui::ET_GESTURE_PINCH_UPDATE:
      return "GESTURE_PINCH_UPDATE";

    case ui::ET_GESTURE_TAP:
      return "GESTURE_TAP";

    case ui::ET_GESTURE_TAP_DOWN:
      return "GESTURE_TAP_DOWN";

    case ui::ET_GESTURE_TAP_CANCEL:
      return "GESTURE_TAP_CANCEL";

    case ui::ET_GESTURE_SHOW_PRESS:
      return "GESTURE_SHOW_PRESS";

    case ui::ET_GESTURE_BEGIN:
      return "GESTURE_BEGIN";

    case ui::ET_GESTURE_END:
      return "GESTURE_END";

    default:
      // We should explicitly require each event type.
      NOTREACHED();
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

// Verifies a repost mouse event targets the window with capture (if there is
// one).
TEST_F(RootWindowTest, RepostTargetsCaptureWindow) {
  // Set capture on |window| generate a mouse event (that is reposted) and not
  // over |window| and verify |window| gets it (|window| gets it because it has
  // capture).
  EXPECT_FALSE(Env::GetInstance()->IsMouseButtonDown());
  scoped_ptr<Window> window(CreateNormalWindow(1, root_window(), NULL));
  window->SetBounds(gfx::Rect(20, 20, 40, 30));
  EventFilterRecorder* recorder = new EventFilterRecorder;
  window->SetEventFilter(recorder);  // Takes ownership.
  window->SetCapture();
  const ui::MouseEvent press_event(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
      ui::EF_LEFT_MOUSE_BUTTON);
  dispatcher()->RepostEvent(press_event);
  RunAllPendingInMessageLoop();  // Necessitated by RepostEvent().
  // Mouse moves/enters may be generated. We only care about a pressed.
  EXPECT_TRUE(EventTypesToString(recorder->events()).find("MOUSE_PRESSED") !=
              std::string::npos) << EventTypesToString(recorder->events());
}

TEST_F(RootWindowTest, MouseMovesHeld) {
  EventFilterRecorder* filter = new EventFilterRecorder;
  root_window()->SetEventFilter(filter);  // passes ownership

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  ui::MouseEvent mouse_move_event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                                  gfx::Point(0, 0), 0);
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_move_event);
  // Discard MOUSE_ENTER.
  filter->events().clear();

  dispatcher()->HoldPointerMoves();

  // Check that we don't immediately dispatch the MOUSE_DRAGGED event.
  ui::MouseEvent mouse_dragged_event(ui::ET_MOUSE_DRAGGED, gfx::Point(0, 0),
                                     gfx::Point(0, 0), 0);
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event);
  EXPECT_TRUE(filter->events().empty());

  // Check that we do dispatch the held MOUSE_DRAGGED event before another type
  // of event.
  ui::MouseEvent mouse_pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0),
                                     gfx::Point(0, 0), 0);
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(filter->events()));
  filter->events().clear();

  // Check that we coalesce held MOUSE_DRAGGED events.
  ui::MouseEvent mouse_dragged_event2(ui::ET_MOUSE_DRAGGED, gfx::Point(1, 1),
                                      gfx::Point(1, 1), 0);
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event);
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event2);
  EXPECT_TRUE(filter->events().empty());
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(filter->events()));
  filter->events().clear();

  // Check that on ReleasePointerMoves, held events are not dispatched
  // immediately, but posted instead.
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event);
  dispatcher()->ReleasePointerMoves();
  EXPECT_TRUE(filter->events().empty());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("MOUSE_DRAGGED", EventTypesToString(filter->events()));
  filter->events().clear();

  // However if another message comes in before the dispatch of the posted
  // event, check that the posted event is dispatched before this new event.
  dispatcher()->HoldPointerMoves();
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event);
  dispatcher()->ReleasePointerMoves();
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_pressed_event);
  EXPECT_EQ("MOUSE_DRAGGED MOUSE_PRESSED",
            EventTypesToString(filter->events()));
  filter->events().clear();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(filter->events().empty());

  // Check that if the other message is another MOUSE_DRAGGED, we still coalesce
  // them.
  dispatcher()->HoldPointerMoves();
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event);
  dispatcher()->ReleasePointerMoves();
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_dragged_event2);
  EXPECT_EQ("MOUSE_DRAGGED", EventTypesToString(filter->events()));
  filter->events().clear();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(filter->events().empty());
}

TEST_F(RootWindowTest, TouchMovesHeld) {
  EventFilterRecorder* filter = new EventFilterRecorder;
  root_window()->SetEventFilter(filter);  // passes ownership

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  // Starting the touch and throwing out the first few events, since the system
  // is going to generate synthetic mouse events that are not relevant to the
  // test.
  ui::TouchEvent touch_pressed_event(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0),
                                     0, base::TimeDelta());
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(
      &touch_pressed_event);
  RunAllPendingInMessageLoop();
  filter->events().clear();

  dispatcher()->HoldPointerMoves();

  // Check that we don't immediately dispatch the TOUCH_MOVED event.
  ui::TouchEvent touch_moved_event(ui::ET_TOUCH_MOVED, gfx::Point(0, 0),
                                   0, base::TimeDelta());
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(
      &touch_moved_event);
  EXPECT_TRUE(filter->events().empty());

  // Check that on ReleasePointerMoves, held events are not dispatched
  // immediately, but posted instead.
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(
      &touch_moved_event);
  dispatcher()->ReleasePointerMoves();
  EXPECT_TRUE(filter->events().empty());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("TOUCH_MOVED", EventTypesToString(filter->events()));
  filter->events().clear();

  // If another touch event occurs then the held touch should be dispatched
  // immediately before it.
  ui::TouchEvent touch_released_event(ui::ET_TOUCH_RELEASED, gfx::Point(0, 0),
                                      0, base::TimeDelta());
  filter->events().clear();
  dispatcher()->HoldPointerMoves();
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(
      &touch_moved_event);
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(
      &touch_released_event);
  EXPECT_EQ("TOUCH_MOVED TOUCH_RELEASED GESTURE_TAP_CANCEL GESTURE_END",
            EventTypesToString(filter->events()));
  filter->events().clear();
  dispatcher()->ReleasePointerMoves();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(filter->events().empty());
}

// Tests that synthetic mouse events are ignored when mouse
// events are disabled.
TEST_F(RootWindowTest, DispatchSyntheticMouseEvents) {
  EventFilterRecorder* filter = new EventFilterRecorder;
  root_window()->SetEventFilter(filter);  // passes ownership

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();
  window->SetCapture();

  test::TestCursorClient cursor_client(root_window());

  // Dispatch a non-synthetic mouse event when mouse events are enabled.
  ui::MouseEvent mouse1(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
                        gfx::Point(10, 10), 0);
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse1);
  EXPECT_FALSE(filter->events().empty());
  filter->events().clear();

  // Dispatch a synthetic mouse event when mouse events are enabled.
  ui::MouseEvent mouse2(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
                        gfx::Point(10, 10), ui::EF_IS_SYNTHESIZED);
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse2);
  EXPECT_FALSE(filter->events().empty());
  filter->events().clear();

  // Dispatch a synthetic mouse event when mouse events are disabled.
  cursor_client.DisableMouseEvents();
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse2);
  EXPECT_TRUE(filter->events().empty());
}

// Tests that a mouse exit is dispatched to the last known cursor location
// when the cursor becomes invisible.
TEST_F(RootWindowTest, DispatchMouseExitWhenCursorHidden) {
  EventFilterRecorder* filter = new EventFilterRecorder;
  root_window()->SetEventFilter(filter);  // passes ownership

  test::TestWindowDelegate delegate;
  gfx::Point window_origin(7, 18);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1234, gfx::Rect(window_origin,
      gfx::Size(100, 100)), root_window()));
  window->Show();

  // Dispatch a mouse move event into the window.
  gfx::Point mouse_location(gfx::Point(15, 25));
  ui::MouseEvent mouse1(ui::ET_MOUSE_MOVED, mouse_location,
                        mouse_location, 0);
  EXPECT_TRUE(filter->events().empty());
  dispatcher()->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse1);
  EXPECT_FALSE(filter->events().empty());
  filter->events().clear();

  // Hide the cursor and verify a mouse exit was dispatched.
  dispatcher()->OnCursorVisibilityChanged(false);
  EXPECT_FALSE(filter->events().empty());
  EXPECT_EQ("MOUSE_EXITED", EventTypesToString(filter->events()));

  // Verify the mouse exit was dispatched at the correct location
  // (in the correct coordinate space).
  int translated_x = mouse_location.x() - window_origin.x();
  int translated_y = mouse_location.y() - window_origin.y();
  gfx::Point translated_point(translated_x, translated_y);
  EXPECT_EQ(filter->mouse_location(0).ToString(), translated_point.ToString());
}

class DeletingEventFilter : public ui::EventHandler {
 public:
  DeletingEventFilter()
      : delete_during_pre_handle_(false) {}
  virtual ~DeletingEventFilter() {}

  void Reset(bool delete_during_pre_handle) {
    delete_during_pre_handle_ = delete_during_pre_handle;
  }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    if (delete_during_pre_handle_)
      delete event->target();
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (delete_during_pre_handle_)
      delete event->target();
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
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    if (delete_during_handle_)
      delete window_;
    got_event_ = true;
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (delete_during_handle_)
      delete window_;
    got_event_ = true;
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
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  DeletingWindowDelegate d11;
  Window* w11 = CreateNormalWindow(11, w1.get(), &d11);
  WindowTracker tracker;
  DeletingEventFilter* w1_filter = new DeletingEventFilter;
  w1->SetEventFilter(w1_filter);
  client::GetFocusClient(w1.get())->FocusWindow(w11);

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
  w11 = CreateNormalWindow(11, w1.get(), &d11);
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
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      event->SetHandled();
      return;
    }

    if (event->type() == ui::ET_GESTURE_TAP) {
      Window* parent = static_cast<Window*>(event->target())->parent();
      parent->parent()->RemoveChild(parent);
      event->SetHandled();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(DetachesParentOnTapDelegate);
};

}  // namespace

// Tests that the gesture recognizer is reset for all child windows when a
// window hides. No expectations, just checks that the test does not crash.
TEST_F(RootWindowTest, GestureRecognizerResetsTargetWhenParentHides) {
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  DetachesParentOnTapDelegate delegate;
  scoped_ptr<Window> parent(CreateNormalWindow(22, w1.get(), NULL));
  Window* child = CreateNormalWindow(11, parent.get(), &delegate);
  test::EventGenerator generator(root_window(), child);
  generator.GestureTapAt(gfx::Point(40, 40));
}

namespace {

// A window delegate that processes nested gestures on tap.
class NestedGestureDelegate : public test::TestWindowDelegate {
 public:
  NestedGestureDelegate(test::EventGenerator* generator,
                        const gfx::Point tap_location)
      : generator_(generator),
        tap_location_(tap_location),
        gesture_end_count_(0) {}
  virtual ~NestedGestureDelegate() {}

  int gesture_end_count() const { return gesture_end_count_; }

 private:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    switch (event->type()) {
      case ui::ET_GESTURE_TAP_DOWN:
        event->SetHandled();
        break;
      case ui::ET_GESTURE_TAP:
        if (generator_)
          generator_->GestureTapAt(tap_location_);
        event->SetHandled();
        break;
      case ui::ET_GESTURE_END:
        ++gesture_end_count_;
        break;
      default:
        break;
    }
  }

  test::EventGenerator* generator_;
  const gfx::Point tap_location_;
  int gesture_end_count_;
  DISALLOW_COPY_AND_ASSIGN(NestedGestureDelegate);
};

}  // namespace

// Tests that gesture end is delivered after nested gesture processing.
TEST_F(RootWindowTest, GestureEndDeliveredAfterNestedGestures) {
  NestedGestureDelegate d1(NULL, gfx::Point());
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), &d1));
  w1->SetBounds(gfx::Rect(0, 0, 100, 100));

  test::EventGenerator nested_generator(root_window(), w1.get());
  NestedGestureDelegate d2(&nested_generator, w1->bounds().CenterPoint());
  scoped_ptr<Window> w2(CreateNormalWindow(1, root_window(), &d2));
  w2->SetBounds(gfx::Rect(100, 0, 100, 100));

  // Tap on w2 which triggers nested gestures for w1.
  test::EventGenerator generator(root_window(), w2.get());
  generator.GestureTapAt(w2->bounds().CenterPoint());

  // Both windows should get their gesture end events.
  EXPECT_EQ(1, d1.gesture_end_count());
  EXPECT_EQ(1, d2.gesture_end_count());
}

// Tests whether we can repost the Tap down gesture event.
TEST_F(RootWindowTest, RepostTapdownGestureTest) {
  EventFilterRecorder* filter = new EventFilterRecorder;
  root_window()->SetEventFilter(filter);  // passes ownership

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  ui::GestureEventDetails details(ui::ET_GESTURE_TAP_DOWN, 0.0f, 0.0f);
  gfx::Point point(10, 10);
  ui::GestureEvent event(
      ui::ET_GESTURE_TAP_DOWN,
      point.x(),
      point.y(),
      0,
      ui::EventTimeForNow(),
      details,
      0);
  dispatcher()->RepostEvent(event);
  RunAllPendingInMessageLoop();
  // TODO(rbyers): Currently disabled - crbug.com/170987
  EXPECT_FALSE(EventTypesToString(filter->events()).find("GESTURE_TAP_DOWN") !=
              std::string::npos);
  filter->events().clear();
}

// This class inherits from the EventFilterRecorder class which provides a
// facility to record events. This class additionally provides a facility to
// repost the ET_GESTURE_TAP_DOWN gesture to the target window and records
// events after that.
class RepostGestureEventRecorder : public EventFilterRecorder {
 public:
  RepostGestureEventRecorder(aura::Window* repost_source,
                             aura::Window* repost_target)
      : repost_source_(repost_source),
        repost_target_(repost_target),
        reposted_(false),
        done_cleanup_(false) {}

  virtual ~RepostGestureEventRecorder() {}

  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE {
    if (reposted_ && event->type() == ui::ET_TOUCH_PRESSED) {
      done_cleanup_ = true;
      events().clear();
    }
    EventFilterRecorder::OnTouchEvent(event);
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    EXPECT_EQ(done_cleanup_ ? repost_target_ : repost_source_, event->target());
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      if (!reposted_) {
        EXPECT_NE(repost_target_, event->target());
        reposted_ = true;
        repost_target_->GetDispatcher()->RepostEvent(*event);
        // Ensure that the reposted gesture event above goes to the
        // repost_target_;
        repost_source_->GetRootWindow()->RemoveChild(repost_source_);
        return;
      }
    }
    EventFilterRecorder::OnGestureEvent(event);
  }

  // Ignore mouse events as they don't fire at all times. This causes
  // the GestureRepostEventOrder test to fail randomly.
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {}

 private:
  aura::Window* repost_source_;
  aura::Window* repost_target_;
  // set to true if we reposted the ET_GESTURE_TAP_DOWN event.
  bool reposted_;
  // set true if we're done cleaning up after hiding repost_source_;
  bool done_cleanup_;
  DISALLOW_COPY_AND_ASSIGN(RepostGestureEventRecorder);
};

// Tests whether events which are generated after the reposted gesture event
// are received after that. In this case the scroll sequence events should
// be received after the reposted gesture event.
TEST_F(RootWindowTest, GestureRepostEventOrder) {
  // Expected events at the end for the repost_target window defined below.
  const char kExpectedTargetEvents[] =
    // TODO)(rbyers): Gesture event reposting is disabled - crbug.com/279039.
    // "GESTURE_BEGIN GESTURE_TAP_DOWN "
    "TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
    "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE TOUCH_MOVED "
    "GESTURE_SCROLL_UPDATE TOUCH_MOVED GESTURE_SCROLL_UPDATE TOUCH_RELEASED "
    "GESTURE_SCROLL_END GESTURE_END";
  // We create two windows.
  // The first window (repost_source) is the one to which the initial tap
  // gesture is sent. It reposts this event to the second window
  // (repost_target).
  // We then generate the scroll sequence for repost_target and look for two
  // ET_GESTURE_TAP_DOWN events in the event list at the end.
  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> repost_target(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  scoped_ptr<aura::Window> repost_source(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 50, 50), root_window()));

  RepostGestureEventRecorder* repost_event_recorder =
      new RepostGestureEventRecorder(repost_source.get(), repost_target.get());
  root_window()->SetEventFilter(repost_event_recorder);  // passes ownership

  // Generate a tap down gesture for the repost_source. This will be reposted
  // to repost_target.
  test::EventGenerator repost_generator(root_window(), repost_source.get());
  repost_generator.GestureTapAt(gfx::Point(40, 40));
  RunAllPendingInMessageLoop();

  test::EventGenerator scroll_generator(root_window(), repost_target.get());
  scroll_generator.GestureScrollSequence(
      gfx::Point(80, 80),
      gfx::Point(100, 100),
      base::TimeDelta::FromMilliseconds(100),
      3);
  RunAllPendingInMessageLoop();

  int tap_down_count = 0;
  for (size_t i = 0; i < repost_event_recorder->events().size(); ++i) {
    if (repost_event_recorder->events()[i] == ui::ET_GESTURE_TAP_DOWN)
      ++tap_down_count;
  }

  // We expect two tap down events. One from the repost and the other one from
  // the scroll sequence posted above.
  // TODO(rbyers): Currently disabled - crbug.com/170987
  EXPECT_EQ(1, tap_down_count);

  EXPECT_EQ(kExpectedTargetEvents,
            EventTypesToString(repost_event_recorder->events()));
}

class OnMouseExitDeletingEventFilter : public EventFilterRecorder {
 public:
  OnMouseExitDeletingEventFilter() : window_to_delete_(NULL) {}
  virtual ~OnMouseExitDeletingEventFilter() {}

  void set_window_to_delete(Window* window_to_delete) {
    window_to_delete_ = window_to_delete;
  }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    EventFilterRecorder::OnMouseEvent(event);
    if (window_to_delete_) {
      delete window_to_delete_;
      window_to_delete_ = NULL;
    }
  }

  Window* window_to_delete_;

  DISALLOW_COPY_AND_ASSIGN(OnMouseExitDeletingEventFilter);
};

// Tests that RootWindow drops mouse-moved event that is supposed to be sent to
// a child, but the child is destroyed because of the synthesized mouse-exit
// event generated on the previous mouse_moved_handler_.
TEST_F(RootWindowTest, DeleteWindowDuringMouseMovedDispatch) {
  // Create window 1 and set its event filter. Window 1 will take ownership of
  // the event filter.
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), NULL));
  OnMouseExitDeletingEventFilter* w1_filter =
      new OnMouseExitDeletingEventFilter();
  w1->SetEventFilter(w1_filter);
  w1->SetBounds(gfx::Rect(20, 20, 60, 60));
  EXPECT_EQ(NULL, dispatcher()->mouse_moved_handler());

  test::EventGenerator generator(root_window(), w1.get());

  // Move mouse over window 1 to set it as the |mouse_moved_handler_| for the
  // root window.
  generator.MoveMouseTo(51, 51);
  EXPECT_EQ(w1.get(), dispatcher()->mouse_moved_handler());

  // Create window 2 under the mouse cursor and stack it above window 1.
  Window* w2 = CreateNormalWindow(2, root_window(), NULL);
  w2->SetBounds(gfx::Rect(30, 30, 40, 40));
  root_window()->StackChildAbove(w2, w1.get());

  // Set window 2 as the window that is to be deleted when a mouse-exited event
  // happens on window 1.
  w1_filter->set_window_to_delete(w2);

  // Move mosue over window 2. This should generate a mouse-exited event for
  // window 1 resulting in deletion of window 2. The original mouse-moved event
  // that was targeted to window 2 should be dropped since window 2 is
  // destroyed. This test passes if no crash happens.
  generator.MoveMouseTo(52, 52);
  EXPECT_EQ(NULL, dispatcher()->mouse_moved_handler());

  // Check events received by window 1.
  EXPECT_EQ("MOUSE_ENTERED MOUSE_MOVED MOUSE_EXITED",
            EventTypesToString(w1_filter->events()));
}

namespace {

// Used to track if OnWindowDestroying() is invoked and if there is a valid
// RootWindow at such time.
class ValidRootDuringDestructionWindowObserver : public aura::WindowObserver {
 public:
  ValidRootDuringDestructionWindowObserver(bool* got_destroying,
                                           bool* has_valid_root)
      : got_destroying_(got_destroying),
        has_valid_root_(has_valid_root) {
  }

  // WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    *got_destroying_ = true;
    *has_valid_root_ = (window->GetRootWindow() != NULL);
  }

 private:
  bool* got_destroying_;
  bool* has_valid_root_;

  DISALLOW_COPY_AND_ASSIGN(ValidRootDuringDestructionWindowObserver);
};

}  // namespace

#if defined(USE_OZONE)
// Creating multiple RootWindowHostOzone instances is broken.
#define MAYBE_ValidRootDuringDestruction DISABLED_ValidRootDuringDestruction
#else
#define MAYBE_ValidRootDuringDestruction ValidRootDuringDestruction
#endif

// Verifies GetRootWindow() from ~Window returns a valid root.
TEST_F(RootWindowTest, MAYBE_ValidRootDuringDestruction) {
  bool got_destroying = false;
  bool has_valid_root = false;
  ValidRootDuringDestructionWindowObserver observer(&got_destroying,
                                                    &has_valid_root);
  {
    scoped_ptr<RootWindow> root_window(
        new RootWindow(RootWindow::CreateParams(gfx::Rect(0, 0, 100, 100))));
    root_window->Init();
    // Owned by RootWindow.
    Window* w1 = CreateNormalWindow(1, root_window->window(), NULL);
    w1->AddObserver(&observer);
  }
  EXPECT_TRUE(got_destroying);
  EXPECT_TRUE(has_valid_root);
}

namespace {

// See description above DontResetHeldEvent for details.
class DontResetHeldEventWindowDelegate : public test::TestWindowDelegate {
 public:
  explicit DontResetHeldEventWindowDelegate(aura::Window* root)
      : root_(root),
        mouse_event_count_(0) {}
  virtual ~DontResetHeldEventWindowDelegate() {}

  int mouse_event_count() const { return mouse_event_count_; }

  // TestWindowDelegate:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if ((event->flags() & ui::EF_SHIFT_DOWN) != 0 &&
        mouse_event_count_++ == 0) {
      ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED,
                                 gfx::Point(10, 10), gfx::Point(10, 10),
                                 ui::EF_SHIFT_DOWN);
      root_->GetDispatcher()->RepostEvent(mouse_event);
    }
  }

 private:
  Window* root_;
  int mouse_event_count_;

  DISALLOW_COPY_AND_ASSIGN(DontResetHeldEventWindowDelegate);
};

}  // namespace

// Verifies RootWindow doesn't reset |RootWindow::held_repostable_event_| after
// dispatching. This is done by using DontResetHeldEventWindowDelegate, which
// tracks the number of events with ui::EF_SHIFT_DOWN set (all reposted events
// have EF_SHIFT_DOWN). When the first event is seen RepostEvent() is used to
// schedule another reposted event.
TEST_F(RootWindowTest, DontResetHeldEvent) {
  DontResetHeldEventWindowDelegate delegate(root_window());
  scoped_ptr<Window> w1(CreateNormalWindow(1, root_window(), &delegate));
  RootWindowHostDelegate* root_window_delegate =
      static_cast<RootWindowHostDelegate*>(root_window()->GetDispatcher());
  w1->SetBounds(gfx::Rect(0, 0, 40, 40));
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED,
                         gfx::Point(10, 10), gfx::Point(10, 10),
                         ui::EF_SHIFT_DOWN);
  root_window()->GetDispatcher()->RepostEvent(pressed);
  ui::MouseEvent pressed2(ui::ET_MOUSE_PRESSED,
                          gfx::Point(10, 10), gfx::Point(10, 10), 0);
  // Invoke OnHostMouseEvent() to flush event scheduled by way of RepostEvent().
  root_window_delegate->OnHostMouseEvent(&pressed2);
  // Delegate should have seen reposted event (identified by way of
  // EF_SHIFT_DOWN). Invoke OnHostMouseEvent() to flush the second
  // RepostedEvent().
  EXPECT_EQ(1, delegate.mouse_event_count());
  root_window_delegate->OnHostMouseEvent(&pressed2);
  EXPECT_EQ(2, delegate.mouse_event_count());
}

namespace {

// See description above DeleteRootFromHeldMouseEvent for details.
class DeleteRootFromHeldMouseEventDelegate : public test::TestWindowDelegate {
 public:
  explicit DeleteRootFromHeldMouseEventDelegate(aura::RootWindow* root)
      : root_(root),
        got_mouse_event_(false),
        got_destroy_(false) {
  }
  virtual ~DeleteRootFromHeldMouseEventDelegate() {}

  bool got_mouse_event() const { return got_mouse_event_; }
  bool got_destroy() const { return got_destroy_; }

  // TestWindowDelegate:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if ((event->flags() & ui::EF_SHIFT_DOWN) != 0) {
      got_mouse_event_ = true;
      delete root_;
    }
  }
  virtual void OnWindowDestroyed() OVERRIDE {
    got_destroy_ = true;
  }

 private:
  RootWindow* root_;
  bool got_mouse_event_;
  bool got_destroy_;

  DISALLOW_COPY_AND_ASSIGN(DeleteRootFromHeldMouseEventDelegate);
};

}  // namespace

#if defined(USE_OZONE)
// Creating multiple RootWindowHostOzone instances is broken.
#define MAYBE_DeleteRootFromHeldMouseEvent DISABLED_DeleteRootFromHeldMouseEvent
#else
#define MAYBE_DeleteRootFromHeldMouseEvent DeleteRootFromHeldMouseEvent
#endif

// Verifies if a RootWindow is deleted from dispatching a held mouse event we
// don't crash.
TEST_F(RootWindowTest, MAYBE_DeleteRootFromHeldMouseEvent) {
  // Should be deleted by |delegate|.
  RootWindow* r2 =
      new RootWindow(RootWindow::CreateParams(gfx::Rect(0, 0, 100, 100)));
  r2->Init();
  DeleteRootFromHeldMouseEventDelegate delegate(r2);
  // Owned by |r2|.
  Window* w1 = CreateNormalWindow(1, r2->window(), &delegate);
  w1->SetBounds(gfx::Rect(0, 0, 40, 40));
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED,
                         gfx::Point(10, 10), gfx::Point(10, 10),
                         ui::EF_SHIFT_DOWN);
  r2->RepostEvent(pressed);
  // RunAllPendingInMessageLoop() to make sure the |pressed| is run.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(delegate.got_mouse_event());
  EXPECT_TRUE(delegate.got_destroy());
}

TEST_F(RootWindowTest, WindowHideCancelsActiveTouches) {
  EventFilterRecorder* filter = new EventFilterRecorder;
  root_window()->SetEventFilter(filter);  // passes ownership

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  gfx::Point position1 = root_window()->bounds().origin();
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, position1, 0, base::TimeDelta());
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(&press);

  EXPECT_EQ("TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN",
            EventTypesToString(filter->GetAndResetEvents()));

  window->Hide();

  EXPECT_EQ("TOUCH_CANCELLED GESTURE_TAP_CANCEL GESTURE_END",
            EventTypesToString(filter->events()));
}

TEST_F(RootWindowTest, WindowHideCancelsActiveGestures) {
  EventFilterRecorder* filter = new EventFilterRecorder;
  root_window()->SetEventFilter(filter);  // passes ownership

  test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));

  gfx::Point position1 = root_window()->bounds().origin();
  gfx::Point position2 = root_window()->bounds().CenterPoint();
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, position1, 0, base::TimeDelta());
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(&press);

  ui::TouchEvent move(ui::ET_TOUCH_MOVED, position2, 0, base::TimeDelta());
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(&move);

  ui::TouchEvent press2(ui::ET_TOUCH_PRESSED, position1, 1, base::TimeDelta());
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(&press2);

  EXPECT_EQ("TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
            "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE "
            "TOUCH_PRESSED GESTURE_BEGIN GESTURE_PINCH_BEGIN",
            EventTypesToString(filter->GetAndResetEvents()));

  window->Hide();

  EXPECT_EQ("TOUCH_CANCELLED GESTURE_PINCH_END GESTURE_END TOUCH_CANCELLED "
            "GESTURE_SCROLL_END GESTURE_END",
            EventTypesToString(filter->events()));
}

// Places two windows side by side. Presses down on one window, and starts a
// scroll. Sets capture on the other window and ensures that the "ending" events
// aren't sent to the window which gained capture.
TEST_F(RootWindowTest, EndingEventDoesntRetarget) {
  scoped_ptr<Window> window1(CreateNormalWindow(1, root_window(), NULL));
  window1->SetBounds(gfx::Rect(0, 0, 40, 40));

  scoped_ptr<Window> window2(CreateNormalWindow(2, root_window(), NULL));
  window2->SetBounds(gfx::Rect(40, 0, 40, 40));

  EventFilterRecorder* filter1 = new EventFilterRecorder();
  window1->SetEventFilter(filter1);  // passes ownership
  EventFilterRecorder* filter2 = new EventFilterRecorder();
  window2->SetEventFilter(filter2);  // passes ownership

  gfx::Point position = window1->bounds().origin();
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, position, 0, base::TimeDelta());
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(&press);

  gfx::Point position2 = window1->bounds().CenterPoint();
  ui::TouchEvent move(ui::ET_TOUCH_MOVED, position2, 0, base::TimeDelta());
  dispatcher()->AsRootWindowHostDelegate()->OnHostTouchEvent(&move);

  window2->SetCapture();

  EXPECT_EQ("TOUCH_PRESSED GESTURE_BEGIN GESTURE_TAP_DOWN TOUCH_MOVED "
            "GESTURE_TAP_CANCEL GESTURE_SCROLL_BEGIN GESTURE_SCROLL_UPDATE "
            "TOUCH_CANCELLED GESTURE_SCROLL_END GESTURE_END",
            EventTypesToString(filter1->events()));

  EXPECT_TRUE(filter2->events().empty());
}

class ExitMessageLoopOnMousePress : public test::TestEventHandler {
 public:
  ExitMessageLoopOnMousePress() {}
  virtual ~ExitMessageLoopOnMousePress() {}

 protected:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    test::TestEventHandler::OnMouseEvent(event);
    if (event->type() == ui::ET_MOUSE_PRESSED)
      base::MessageLoopForUI::current()->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExitMessageLoopOnMousePress);
};

class RootWindowTestWithMessageLoop : public RootWindowTest {
 public:
  RootWindowTestWithMessageLoop() {}
  virtual ~RootWindowTestWithMessageLoop() {}

  void RunTest() {
    // Start a nested message-loop, post an event to be dispatched, and then
    // terminate the message-loop. When the message-loop unwinds and gets back,
    // the reposted event should not have fired.
    ui::MouseEvent mouse(ui::ET_MOUSE_PRESSED, gfx::Point(10, 10),
                         gfx::Point(10, 10), ui::EF_NONE);
    message_loop()->PostTask(FROM_HERE,
                             base::Bind(&RootWindow::RepostEvent,
                                        base::Unretained(dispatcher()),
                                        mouse));
    message_loop()->PostTask(FROM_HERE,
                             message_loop()->QuitClosure());

    base::MessageLoop::ScopedNestableTaskAllower allow(message_loop());
    base::RunLoop loop;
    loop.Run();
    EXPECT_EQ(0, handler_.num_mouse_events());

    // Let the current message-loop run. The event-handler will terminate the
    // message-loop when it receives the reposted event.
  }

  base::MessageLoop* message_loop() {
    return base::MessageLoopForUI::current();
  }

 protected:
  virtual void SetUp() OVERRIDE {
    RootWindowTest::SetUp();
    window_.reset(CreateNormalWindow(1, root_window(), NULL));
    window_->AddPreTargetHandler(&handler_);
  }

  virtual void TearDown() OVERRIDE {
    window_.reset();
    RootWindowTest::TearDown();
  }

 private:
  scoped_ptr<Window> window_;
  ExitMessageLoopOnMousePress handler_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowTestWithMessageLoop);
};

TEST_F(RootWindowTestWithMessageLoop, EventRepostedInNonNestedLoop) {
  CHECK(!message_loop()->is_running());
  // Perform the test in a callback, so that it runs after the message-loop
  // starts.
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&RootWindowTestWithMessageLoop::RunTest,
                                      base::Unretained(this)));
  message_loop()->Run();
}

}  // namespace aura
