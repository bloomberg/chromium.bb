// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/event_generator.h"

#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"
#include "ui/gfx/vector2d_conversions.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_WIN)
#include "ui/base/keycodes/keyboard_code_conversion.h"
#endif

namespace aura {
namespace test {
namespace {

class DefaultEventGeneratorDelegate : public EventGeneratorDelegate {
 public:
  explicit DefaultEventGeneratorDelegate(RootWindow* root_window)
      : root_window_(root_window) {}
  virtual ~DefaultEventGeneratorDelegate() {}

  // EventGeneratorDelegate overrides:
  RootWindow* GetRootWindowAt(const gfx::Point& point) const OVERRIDE {
    return root_window_;
  }

  client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const OVERRIDE {
    return NULL;
  }

 private:
  RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DefaultEventGeneratorDelegate);
};

class TestKeyEvent : public ui::KeyEvent {
 public:
  TestKeyEvent(const base::NativeEvent& native_event, int flags, bool is_char)
      : KeyEvent(native_event, is_char) {
    set_flags(flags);
  }
};

class TestTouchEvent : public ui::TouchEvent {
 public:
  TestTouchEvent(ui::EventType type,
                 const gfx::Point& root_location,
                 int flags)
      : TouchEvent(type, root_location, 0, ui::EventTimeForNow()) {
    set_flags(flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTouchEvent);
};

const int kAllButtonMask = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON;

}  // namespace

EventGenerator::EventGenerator(RootWindow* root_window)
    : delegate_(new DefaultEventGeneratorDelegate(root_window)),
      current_root_window_(delegate_->GetRootWindowAt(current_location_)),
      flags_(0),
      grab_(false) {
}

EventGenerator::EventGenerator(RootWindow* root_window, const gfx::Point& point)
    : delegate_(new DefaultEventGeneratorDelegate(root_window)),
      current_location_(point),
      current_root_window_(delegate_->GetRootWindowAt(current_location_)),
      flags_(0),
      grab_(false) {
}

EventGenerator::EventGenerator(RootWindow* root_window, Window* window)
    : delegate_(new DefaultEventGeneratorDelegate(root_window)),
      current_location_(CenterOfWindow(window)),
      current_root_window_(delegate_->GetRootWindowAt(current_location_)),
      flags_(0),
      grab_(false) {
}

EventGenerator::EventGenerator(EventGeneratorDelegate* delegate)
    : delegate_(delegate),
      current_root_window_(delegate_->GetRootWindowAt(current_location_)),
      flags_(0),
      grab_(false) {
}

EventGenerator::~EventGenerator() {
}

void EventGenerator::PressLeftButton() {
  PressButton(ui::EF_LEFT_MOUSE_BUTTON);
}

void EventGenerator::ReleaseLeftButton() {
  ReleaseButton(ui::EF_LEFT_MOUSE_BUTTON);
}

void EventGenerator::ClickLeftButton() {
  PressLeftButton();
  ReleaseLeftButton();
}

void EventGenerator::DoubleClickLeftButton() {
  flags_ |= ui::EF_IS_DOUBLE_CLICK;
  PressLeftButton();
  flags_ ^= ui::EF_IS_DOUBLE_CLICK;
  ReleaseLeftButton();
}

void EventGenerator::PressRightButton() {
  PressButton(ui::EF_RIGHT_MOUSE_BUTTON);
}

void EventGenerator::ReleaseRightButton() {
  ReleaseButton(ui::EF_RIGHT_MOUSE_BUTTON);
}

void EventGenerator::MoveMouseTo(const gfx::Point& point, int count) {
  DCHECK_GT(count, 0);
  const ui::EventType event_type = (flags_ & ui::EF_LEFT_MOUSE_BUTTON) ?
      ui::ET_MOUSE_DRAGGED : ui::ET_MOUSE_MOVED;

  gfx::Vector2dF diff(point - current_location_);
  for (float i = 1; i <= count; i++) {
    gfx::Vector2dF step(diff);
    step.Scale(i / count);
    gfx::Point move_point = current_location_ + gfx::ToRoundedVector2d(step);
    if (!grab_)
      UpdateCurrentRootWindow(move_point);
    ConvertPointToTarget(current_root_window_, &move_point);
    ui::MouseEvent mouseev(event_type, move_point, move_point, flags_);
    Dispatch(mouseev);
  }
  current_location_ = point;
}

void EventGenerator::MoveMouseRelativeTo(const Window* window,
                                         const gfx::Point& point_in_parent) {
  gfx::Point point(point_in_parent);
  ConvertPointFromTarget(window, &point);
  MoveMouseTo(point);
}

void EventGenerator::DragMouseTo(const gfx::Point& point) {
  PressLeftButton();
  MoveMouseTo(point);
  ReleaseLeftButton();
}

void EventGenerator::MoveMouseToCenterOf(Window* window) {
  MoveMouseTo(CenterOfWindow(window));
}

void EventGenerator::PressTouch() {
  TestTouchEvent touchev(
      ui::ET_TOUCH_PRESSED, GetLocationInCurrentRoot(), flags_);
  Dispatch(touchev);
}

void EventGenerator::MoveTouch(const gfx::Point& point) {
  TestTouchEvent touchev(ui::ET_TOUCH_MOVED, point, flags_);
  Dispatch(touchev);

  current_location_ = point;
  if (!grab_)
    UpdateCurrentRootWindow(point);
}

void EventGenerator::ReleaseTouch() {
  TestTouchEvent touchev(
      ui::ET_TOUCH_RELEASED, GetLocationInCurrentRoot(), flags_);
  Dispatch(touchev);
}

void EventGenerator::PressMoveAndReleaseTouchTo(const gfx::Point& point) {
  PressTouch();
  MoveTouch(point);
  ReleaseTouch();
}

void EventGenerator::PressMoveAndReleaseTouchToCenterOf(Window* window) {
  PressMoveAndReleaseTouchTo(CenterOfWindow(window));
}

void EventGenerator::GestureTapAt(const gfx::Point& location) {
  const int kTouchId = 2;
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED,
                       location,
                       kTouchId,
                       ui::EventTimeForNow());
  Dispatch(press);

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, location, kTouchId,
      press.time_stamp() + base::TimeDelta::FromMilliseconds(50));
  Dispatch(release);
}

void EventGenerator::GestureTapDownAndUp(const gfx::Point& location) {
  const int kTouchId = 3;
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED,
                       location,
                       kTouchId,
                       ui::EventTimeForNow());
  Dispatch(press);

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, location, kTouchId,
      press.time_stamp() + base::TimeDelta::FromMilliseconds(1000));
  Dispatch(release);
}

void EventGenerator::GestureScrollSequence(const gfx::Point& start,
                                           const gfx::Point& end,
                                           const base::TimeDelta& step_delay,
                                           int steps) {
  const int kTouchId = 5;
  base::TimeDelta timestamp = ui::EventTimeForNow();
  ui::TouchEvent press(ui::ET_TOUCH_PRESSED, start, kTouchId, timestamp);
  Dispatch(press);

  int dx = (end.x() - start.x()) / steps;
  int dy = (end.y() - start.y()) / steps;
  gfx::Point location = start;
  for (int i = 0; i < steps; ++i) {
    location.Offset(dx, dy);
    timestamp += step_delay;
    ui::TouchEvent move(ui::ET_TOUCH_MOVED, location, kTouchId, timestamp);
    Dispatch(move);
  }

  ui::TouchEvent release(ui::ET_TOUCH_RELEASED, end, kTouchId, timestamp);
  Dispatch(release);
}

void EventGenerator::GestureMultiFingerScroll(int count,
                                              const gfx::Point* start,
                                              int event_separation_time_ms,
                                              int steps,
                                              int move_x,
                                              int move_y) {
  const int kMaxTouchPoints = 10;
  gfx::Point points[kMaxTouchPoints];
  CHECK_LE(count, kMaxTouchPoints);
  CHECK_GT(steps, 0);

  int delta_x = move_x / steps;
  int delta_y = move_y / steps;

  base::TimeDelta press_time = ui::EventTimeForNow();
  for (int i = 0; i < count; ++i) {
    points[i] = start[i];
    ui::TouchEvent press(ui::ET_TOUCH_PRESSED, points[i], i, press_time);
    Dispatch(press);
  }

  for (int step = 0; step < steps; ++step) {
    base::TimeDelta move_time = press_time +
        base::TimeDelta::FromMilliseconds(event_separation_time_ms * step);
    for (int i = 0; i < count; ++i) {
      points[i].Offset(delta_x, delta_y);
      ui::TouchEvent move(ui::ET_TOUCH_MOVED, points[i], i, move_time);
      Dispatch(move);
    }
  }

  base::TimeDelta release_time = press_time +
      base::TimeDelta::FromMilliseconds(event_separation_time_ms * steps);
  for (int i = 0; i < count; ++i) {
    ui::TouchEvent release(
        ui::ET_TOUCH_RELEASED, points[i], i, release_time);
    Dispatch(release);
  }
}

void EventGenerator::PressKey(ui::KeyboardCode key_code, int flags) {
  DispatchKeyEvent(true, key_code, flags);
}

void EventGenerator::ReleaseKey(ui::KeyboardCode key_code, int flags) {
  DispatchKeyEvent(false, key_code, flags);
}

void EventGenerator::Dispatch(ui::Event& event) {
  switch (event.type()) {
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
      current_root_window_->AsRootWindowHostDelegate()->OnHostKeyEvent(
          static_cast<ui::KeyEvent*>(&event));
      break;
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSEWHEEL:
      current_root_window_->AsRootWindowHostDelegate()->OnHostMouseEvent(
          static_cast<ui::MouseEvent*>(&event));
      break;
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_MOVED:
    case ui::ET_TOUCH_STATIONARY:
    case ui::ET_TOUCH_CANCELLED:
      current_root_window_->AsRootWindowHostDelegate()->OnHostTouchEvent(
          static_cast<ui::TouchEvent*>(&event));
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
}

void EventGenerator::DispatchKeyEvent(bool is_press,
                                      ui::KeyboardCode key_code,
                                      int flags) {
#if defined(OS_WIN)
  UINT key_press = WM_KEYDOWN;
  uint16 character = ui::GetCharacterFromKeyCode(key_code, flags);
  if (is_press && character) {
    MSG native_event = { NULL, WM_KEYDOWN, key_code, 0 };
    TestKeyEvent keyev(native_event, flags, false);
    Dispatch(keyev);
    // On Windows, WM_KEYDOWN event is followed by WM_CHAR with a character
    // if the key event cooresponds to a real character.
    key_press = WM_CHAR;
    key_code = static_cast<ui::KeyboardCode>(character);
  }
  MSG native_event =
      { NULL, (is_press ? key_press : WM_KEYUP), key_code, 0 };
  TestKeyEvent keyev(native_event, flags, key_press == WM_CHAR);
#else
  ui::EventType type = is_press ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED;
#if defined(USE_X11)
  scoped_ptr<XEvent> native_event(new XEvent);
  ui::InitXKeyEventForTesting(type, key_code, flags, native_event.get());
  TestKeyEvent keyev(native_event.get(), flags, false);
#else
  ui::KeyEvent keyev(type, key_code, flags, false);
#endif  // USE_X11
#endif  // OS_WIN
  Dispatch(keyev);
}

void EventGenerator::UpdateCurrentRootWindow(const gfx::Point& point) {
  current_root_window_ = delegate_->GetRootWindowAt(point);
}

void EventGenerator::PressButton(int flag) {
  if (!(flags_ & flag)) {
    flags_ |= flag;
    grab_ = flags_ & kAllButtonMask;
    gfx::Point location = GetLocationInCurrentRoot();
    ui::MouseEvent mouseev(ui::ET_MOUSE_PRESSED, location, location, flags_);
    Dispatch(mouseev);
  }
}

void EventGenerator::ReleaseButton(int flag) {
  if (flags_ & flag) {
    gfx::Point location = GetLocationInCurrentRoot();
    ui::MouseEvent mouseev(ui::ET_MOUSE_RELEASED, location,
                           location, flags_);
    Dispatch(mouseev);
    flags_ ^= flag;
  }
  grab_ = flags_ & kAllButtonMask;
}

void EventGenerator::ConvertPointFromTarget(const aura::Window* target,
                                            gfx::Point* point) const {
  DCHECK(point);
  aura::client::ScreenPositionClient* client =
      delegate_->GetScreenPositionClient(target);
  if (client)
    client->ConvertPointToScreen(target, point);
  else
    aura::Window::ConvertPointToTarget(target, target->GetRootWindow(), point);
}

void EventGenerator::ConvertPointToTarget(const aura::Window* target,
                                          gfx::Point* point) const {
  DCHECK(point);
  aura::client::ScreenPositionClient* client =
      delegate_->GetScreenPositionClient(target);
  if (client)
    client->ConvertPointFromScreen(target, point);
  else
    aura::Window::ConvertPointToTarget(target->GetRootWindow(), target, point);
}

gfx::Point EventGenerator::GetLocationInCurrentRoot() const {
  gfx::Point p(current_location_);
  ConvertPointToTarget(current_root_window_, &p);
  return p;
}

gfx::Point EventGenerator::CenterOfWindow(const Window* window) const {
  gfx::Point center = gfx::Rect(window->bounds().size()).CenterPoint();
  ConvertPointFromTarget(window, &center);
  return center;
}

}  // namespace test
}  // namespace aura
