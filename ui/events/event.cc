// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event.h"

#include <cmath>
#include <cstring>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

#if defined(USE_X11)
#include "ui/events/keycodes/keyboard_code_conversion_x.h"  // nogncheck
#include "ui/gfx/x/x11.h"                                   // nogncheck
#elif defined(USE_OZONE)
#include "ui/events/ozone/layout/keyboard_layout_engine.h"  // nogncheck
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"  // nogncheck
#endif

#if defined(OS_WIN)
#include "ui/events/keycodes/platform_key_map_win.h"
#endif

namespace ui {
namespace {

#if defined(USE_X11)
bool X11EventHasNonStandardState(const PlatformEvent& event) {
  const unsigned int kAllStateMask =
      Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask |
      Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask | ShiftMask |
      LockMask | ControlMask | AnyModifier;

  return event && (event->xkey.state & ~kAllStateMask) != 0;
}
#endif

constexpr int kChangedButtonFlagMask =
    ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON |
    ui::EF_RIGHT_MOUSE_BUTTON | ui::EF_BACK_MOUSE_BUTTON |
    ui::EF_FORWARD_MOUSE_BUTTON;

SourceEventType EventTypeToLatencySourceEventType(EventType type) {
  switch (type) {
    case ET_UNKNOWN:
    // SourceEventType for PointerEvents/GestureEvents can be either TOUCH or
    // WHEEL. The proper value is assigned in the constructors.
    case ET_POINTER_DOWN:
    case ET_POINTER_MOVED:
    case ET_POINTER_UP:
    case ET_POINTER_CANCELLED:
    case ET_POINTER_ENTERED:
    case ET_POINTER_EXITED:
    case ET_POINTER_CAPTURE_CHANGED:
    case ET_GESTURE_SCROLL_BEGIN:
    case ET_GESTURE_SCROLL_END:
    case ET_GESTURE_SCROLL_UPDATE:
    case ET_GESTURE_TAP:
    case ET_GESTURE_TAP_DOWN:
    case ET_GESTURE_TAP_CANCEL:
    case ET_GESTURE_TAP_UNCONFIRMED:
    case ET_GESTURE_DOUBLE_TAP:
    case ET_GESTURE_BEGIN:
    case ET_GESTURE_END:
    case ET_GESTURE_TWO_FINGER_TAP:
    case ET_GESTURE_PINCH_BEGIN:
    case ET_GESTURE_PINCH_END:
    case ET_GESTURE_PINCH_UPDATE:
    case ET_GESTURE_LONG_PRESS:
    case ET_GESTURE_LONG_TAP:
    case ET_GESTURE_SWIPE:
    case ET_GESTURE_SHOW_PRESS:
    // Flings can be GestureEvents too.
    case ET_SCROLL_FLING_START:
    case ET_SCROLL_FLING_CANCEL:
      return SourceEventType::UNKNOWN;

    case ui::ET_KEY_PRESSED:
      return ui::SourceEventType::KEY_PRESS;

    case ET_MOUSE_PRESSED:
    case ET_MOUSE_DRAGGED:
    case ET_MOUSE_RELEASED:
    case ET_MOUSE_MOVED:
    case ET_MOUSE_ENTERED:
    case ET_MOUSE_EXITED:
    // We measure latency for key presses, not key releases. Most behavior is
    // keyed off of presses, and release latency is higher than press latency as
    // it's impacted by event handling of the press event.
    case ET_KEY_RELEASED:
    case ET_MOUSE_CAPTURE_CHANGED:
    case ET_DROP_TARGET_EVENT:
    case ET_CANCEL_MODE:
    case ET_UMA_DATA:
      return SourceEventType::OTHER;

    case ET_TOUCH_RELEASED:
    case ET_TOUCH_PRESSED:
    case ET_TOUCH_MOVED:
    case ET_TOUCH_CANCELLED:
      return SourceEventType::TOUCH;

    case ET_MOUSEWHEEL:
    case ET_POINTER_WHEEL_CHANGED:
    case ET_SCROLL:
      return SourceEventType::WHEEL;

    case ET_LAST:
      NOTREACHED();
      return SourceEventType::UNKNOWN;
  }
  NOTREACHED();
  return SourceEventType::UNKNOWN;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Event

// static
std::unique_ptr<Event> Event::Clone(const Event& event) {
  if (event.IsKeyEvent()) {
    return std::make_unique<KeyEvent>(static_cast<const KeyEvent&>(event));
  }

  if (event.IsMouseEvent()) {
    if (event.IsMouseWheelEvent()) {
      return std::make_unique<MouseWheelEvent>(
          static_cast<const MouseWheelEvent&>(event));
    }

    return std::make_unique<MouseEvent>(static_cast<const MouseEvent&>(event));
  }

  if (event.IsTouchEvent()) {
    return std::make_unique<TouchEvent>(static_cast<const TouchEvent&>(event));
  }

  if (event.IsGestureEvent()) {
    return std::make_unique<GestureEvent>(
        static_cast<const GestureEvent&>(event));
  }

  if (event.IsPointerEvent()) {
    return std::make_unique<PointerEvent>(
        static_cast<const PointerEvent&>(event));
  }

  if (event.IsScrollEvent()) {
    return std::make_unique<ScrollEvent>(
        static_cast<const ScrollEvent&>(event));
  }

  return base::WrapUnique(new Event(event));
}

Event::~Event() {
  if (delete_native_event_)
    ReleaseCopiedNativeEvent(native_event_);
}

const char* Event::GetName() const {
  return EventTypeName(type_);
}

bool Event::IsMousePointerEvent() const {
  return IsPointerEvent() &&
         AsPointerEvent()->pointer_details().pointer_type ==
             EventPointerType::POINTER_TYPE_MOUSE;
}

bool Event::IsTouchPointerEvent() const {
  return IsPointerEvent() &&
         AsPointerEvent()->pointer_details().pointer_type ==
             EventPointerType::POINTER_TYPE_TOUCH;
}

bool Event::IsPenPointerEvent() const {
  return IsPointerEvent() && AsPointerEvent()->pointer_details().pointer_type ==
                                 EventPointerType::POINTER_TYPE_PEN;
}

CancelModeEvent* Event::AsCancelModeEvent() {
  CHECK(IsCancelModeEvent());
  return static_cast<CancelModeEvent*>(this);
}

const CancelModeEvent* Event::AsCancelModeEvent() const {
  CHECK(IsCancelModeEvent());
  return static_cast<const CancelModeEvent*>(this);
}

GestureEvent* Event::AsGestureEvent() {
  CHECK(IsGestureEvent());
  return static_cast<GestureEvent*>(this);
}

const GestureEvent* Event::AsGestureEvent() const {
  CHECK(IsGestureEvent());
  return static_cast<const GestureEvent*>(this);
}

KeyEvent* Event::AsKeyEvent() {
  CHECK(IsKeyEvent());
  return static_cast<KeyEvent*>(this);
}

const KeyEvent* Event::AsKeyEvent() const {
  CHECK(IsKeyEvent());
  return static_cast<const KeyEvent*>(this);
}

LocatedEvent* Event::AsLocatedEvent() {
  CHECK(IsLocatedEvent());
  return static_cast<LocatedEvent*>(this);
}

const LocatedEvent* Event::AsLocatedEvent() const {
  CHECK(IsLocatedEvent());
  return static_cast<const LocatedEvent*>(this);
}

MouseEvent* Event::AsMouseEvent() {
  CHECK(IsMouseEvent());
  return static_cast<MouseEvent*>(this);
}

const MouseEvent* Event::AsMouseEvent() const {
  CHECK(IsMouseEvent());
  return static_cast<const MouseEvent*>(this);
}

MouseWheelEvent* Event::AsMouseWheelEvent() {
  CHECK(IsMouseWheelEvent());
  return static_cast<MouseWheelEvent*>(this);
}

const MouseWheelEvent* Event::AsMouseWheelEvent() const {
  CHECK(IsMouseWheelEvent());
  return static_cast<const MouseWheelEvent*>(this);
}

PointerEvent* Event::AsPointerEvent() {
  CHECK(IsPointerEvent());
  return static_cast<PointerEvent*>(this);
}

const PointerEvent* Event::AsPointerEvent() const {
  CHECK(IsPointerEvent());
  return static_cast<const PointerEvent*>(this);
}

ScrollEvent* Event::AsScrollEvent() {
  CHECK(IsScrollEvent());
  return static_cast<ScrollEvent*>(this);
}

const ScrollEvent* Event::AsScrollEvent() const {
  CHECK(IsScrollEvent());
  return static_cast<const ScrollEvent*>(this);
}

TouchEvent* Event::AsTouchEvent() {
  CHECK(IsTouchEvent());
  return static_cast<TouchEvent*>(this);
}

const TouchEvent* Event::AsTouchEvent() const {
  CHECK(IsTouchEvent());
  return static_cast<const TouchEvent*>(this);
}

bool Event::HasNativeEvent() const {
  PlatformEvent null_event;
  std::memset(&null_event, 0, sizeof(null_event));
  return !!std::memcmp(&native_event_, &null_event, sizeof(null_event));
}

void Event::StopPropagation() {
  // TODO(sad): Re-enable these checks once View uses dispatcher to dispatch
  // events.
  // CHECK(phase_ != EP_PREDISPATCH && phase_ != EP_POSTDISPATCH);
  CHECK(cancelable_);
  result_ = static_cast<EventResult>(result_ | ER_CONSUMED);
}

void Event::SetHandled() {
  // TODO(sad): Re-enable these checks once View uses dispatcher to dispatch
  // events.
  // CHECK(phase_ != EP_PREDISPATCH && phase_ != EP_POSTDISPATCH);
  CHECK(cancelable_);
  result_ = static_cast<EventResult>(result_ | ER_HANDLED);
}

Event::Event(EventType type, base::TimeTicks time_stamp, int flags)
    : type_(type),
      time_stamp_(time_stamp),
      flags_(flags),
      native_event_(PlatformEvent()),
      delete_native_event_(false),
      cancelable_(true),
      target_(NULL),
      phase_(EP_PREDISPATCH),
      result_(ER_UNHANDLED),
      source_device_id_(ED_UNKNOWN_DEVICE) {
  if (type_ < ET_LAST)
    latency()->set_source_event_type(EventTypeToLatencySourceEventType(type));
}

Event::Event(const PlatformEvent& native_event, EventType type, int flags)
    : type_(type),
      time_stamp_(EventTimeFromNative(native_event)),
      flags_(flags),
      native_event_(native_event),
      delete_native_event_(false),
      cancelable_(true),
      target_(NULL),
      phase_(EP_PREDISPATCH),
      result_(ER_UNHANDLED),
      source_device_id_(ED_UNKNOWN_DEVICE) {
  if (type_ < ET_LAST)
    latency()->set_source_event_type(EventTypeToLatencySourceEventType(type));
  ComputeEventLatencyOS(native_event);

#if defined(USE_X11)
  if (native_event->type == GenericEvent) {
    XIDeviceEvent* xiev =
        static_cast<XIDeviceEvent*>(native_event->xcookie.data);
    source_device_id_ = xiev->sourceid;
  }
#endif
#if defined(USE_OZONE)
  source_device_id_ =
      static_cast<const Event*>(native_event)->source_device_id();
#endif
}

Event::Event(const Event& copy)
    : type_(copy.type_),
      time_stamp_(copy.time_stamp_),
      latency_(copy.latency_),
      flags_(copy.flags_),
      native_event_(CopyNativeEvent(copy.native_event_)),
      delete_native_event_(true),
      cancelable_(true),
      target_(NULL),
      phase_(EP_PREDISPATCH),
      result_(ER_UNHANDLED),
      source_device_id_(copy.source_device_id_) {
}

void Event::SetType(EventType type) {
  type_ = type;
  if (type_ < ET_LAST)
    latency()->set_source_event_type(EventTypeToLatencySourceEventType(type));
}

////////////////////////////////////////////////////////////////////////////////
// CancelModeEvent

CancelModeEvent::CancelModeEvent()
    : Event(ET_CANCEL_MODE, base::TimeTicks(), 0) {
  set_cancelable(false);
}

CancelModeEvent::~CancelModeEvent() {
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent

LocatedEvent::~LocatedEvent() = default;

LocatedEvent::LocatedEvent(const PlatformEvent& native_event)
    : Event(native_event,
            EventTypeFromNative(native_event),
            EventFlagsFromNative(native_event)),
      location_(EventLocationFromNative(native_event)),
      root_location_(location_) {}

LocatedEvent::LocatedEvent(EventType type,
                           const gfx::PointF& location,
                           const gfx::PointF& root_location,
                           base::TimeTicks time_stamp,
                           int flags)
    : Event(type, time_stamp, flags),
      location_(location),
      root_location_(root_location) {}

LocatedEvent::LocatedEvent(const LocatedEvent& copy) = default;

void LocatedEvent::UpdateForRootTransform(
    const gfx::Transform& reversed_root_transform,
    const gfx::Transform& reversed_local_transform) {
  if (target()) {
    gfx::Point3F transformed_location(location_);
    reversed_local_transform.TransformPoint(&transformed_location);
    location_ = transformed_location.AsPointF();

    gfx::Point3F transformed_root_location(root_location_);
    reversed_root_transform.TransformPoint(&transformed_root_location);
    root_location_ = transformed_root_location.AsPointF();
  } else {
    // This mirrors what the code previously did.
    gfx::Point3F transformed_location(location_);
    reversed_root_transform.TransformPoint(&transformed_location);
    root_location_ = location_ = transformed_location.AsPointF();
  }
}

////////////////////////////////////////////////////////////////////////////////
// PointerDetails

PointerDetails::PointerDetails() {}

PointerDetails::PointerDetails(EventPointerType pointer_type,
                               PointerId pointer_id)
    : PointerDetails(pointer_type,
                     pointer_id,
                     /* radius_x */ 0.0f,
                     /* radius_y */ 0.0f,
                     /* force */ std::numeric_limits<float>::quiet_NaN()) {}

PointerDetails::PointerDetails(EventPointerType pointer_type,
                               PointerId pointer_id,
                               float radius_x,
                               float radius_y,
                               float force,
                               float twist,
                               float tilt_x,
                               float tilt_y,
                               float tangential_pressure)
    : pointer_type(pointer_type),
      // If we aren't provided with a radius on one axis, use the
      // information from the other axis.
      radius_x(radius_x > 0 ? radius_x : radius_y),
      radius_y(radius_y > 0 ? radius_y : radius_x),
      force(force),
      tilt_x(tilt_x),
      tilt_y(tilt_y),
      tangential_pressure(tangential_pressure),
      twist(twist),
      id(pointer_id) {
  if (pointer_id == PointerDetails::kUnknownPointerId) {
    id = pointer_type == EventPointerType::POINTER_TYPE_MOUSE
             ? MouseEvent::kMousePointerId
             : 0;
  }
}

PointerDetails::PointerDetails(EventPointerType pointer_type,
                               const gfx::Vector2d& pointer_offset,
                               PointerId pointer_id)
    : PointerDetails(pointer_type, pointer_id) {
  offset = pointer_offset;
}

PointerDetails::PointerDetails(const PointerDetails& other) = default;

const PointerId PointerDetails::kUnknownPointerId = -1;

////////////////////////////////////////////////////////////////////////////////
// MouseEvent

MouseEvent::MouseEvent(const PlatformEvent& native_event)
    : LocatedEvent(native_event),
      changed_button_flags_(GetChangedMouseButtonFlagsFromNative(native_event)),
      pointer_details_(GetMousePointerDetailsFromNative(native_event)) {
  latency()->set_source_event_type(ui::SourceEventType::MOUSE);
  latency()->AddLatencyNumberWithTimestamp(
      INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, time_stamp(), 1);
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT);
  if (type() == ET_MOUSE_PRESSED || type() == ET_MOUSE_RELEASED)
    SetClickCount(GetRepeatCount(*this));
}

MouseEvent::MouseEvent(const PointerEvent& pointer_event)
    : LocatedEvent(pointer_event),
      changed_button_flags_(pointer_event.changed_button_flags()),
      pointer_details_(pointer_event.pointer_details()) {
  latency()->set_source_event_type(ui::SourceEventType::MOUSE);
  DCHECK(pointer_event.IsMousePointerEvent());
  switch (pointer_event.type()) {
    case ET_POINTER_DOWN:
      SetType(ET_MOUSE_PRESSED);
      break;

    case ET_POINTER_MOVED:
      if (pointer_event.flags() &
          (EF_LEFT_MOUSE_BUTTON | EF_MIDDLE_MOUSE_BUTTON |
           EF_RIGHT_MOUSE_BUTTON)) {
        SetType(ET_MOUSE_DRAGGED);
      } else {
        SetType(ET_MOUSE_MOVED);
      }
      break;

    case ET_POINTER_ENTERED:
      SetType(ET_MOUSE_ENTERED);
      break;

    case ET_POINTER_EXITED:
      SetType(ET_MOUSE_EXITED);
      break;

    case ET_POINTER_UP:
      SetType(ET_MOUSE_RELEASED);
      break;

    case ET_POINTER_WHEEL_CHANGED:
      // Explicitly not setting a type here. MouseWheelEvent should be converted
      // from PointerEvent using its own ctor.
      break;

    case ET_POINTER_CAPTURE_CHANGED:
      SetType(ET_MOUSE_CAPTURE_CHANGED);
      break;

    default:
      NOTREACHED();
  }
}

MouseEvent::MouseEvent(EventType type,
                       const gfx::Point& location,
                       const gfx::Point& root_location,
                       base::TimeTicks time_stamp,
                       int flags,
                       int changed_button_flags,
                       const PointerDetails& pointer_details)
    : LocatedEvent(type,
                   gfx::PointF(location),
                   gfx::PointF(root_location),
                   time_stamp,
                   flags),
      changed_button_flags_(changed_button_flags),
      pointer_details_(pointer_details) {
  DCHECK_NE(ET_MOUSEWHEEL, type);
  DCHECK_EQ(changed_button_flags_,
            changed_button_flags_ & kChangedButtonFlagMask);
  latency()->set_source_event_type(ui::SourceEventType::MOUSE);
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT);
  if (this->type() == ET_MOUSE_MOVED && IsAnyButton())
    SetType(ET_MOUSE_DRAGGED);
}

MouseEvent::MouseEvent(const MouseEvent& other) = default;

MouseEvent::~MouseEvent() = default;

// static
bool MouseEvent::IsRepeatedClickEvent(const MouseEvent& event1,
                                      const MouseEvent& event2) {
  // These values match the Windows defaults.
  static const int kDoubleClickTimeMS = 500;
  static const int kDoubleClickWidth = 4;
  static const int kDoubleClickHeight = 4;

  if (event1.type() != ET_MOUSE_PRESSED ||
      event2.type() != ET_MOUSE_PRESSED)
    return false;

  // Compare flags, but ignore EF_IS_DOUBLE_CLICK to allow triple clicks.
  if ((event1.flags() & ~EF_IS_DOUBLE_CLICK) !=
      (event2.flags() & ~EF_IS_DOUBLE_CLICK))
    return false;

  // The new event has been created from the same native event.
  if (event1.time_stamp() == event2.time_stamp())
    return false;

  base::TimeDelta time_difference = event2.time_stamp() - event1.time_stamp();

  if (time_difference.InMilliseconds() > kDoubleClickTimeMS)
    return false;

  if (std::abs(event2.x() - event1.x()) > kDoubleClickWidth / 2)
    return false;

  if (std::abs(event2.y() - event1.y()) > kDoubleClickHeight / 2)
    return false;

  return true;
}

// static
int MouseEvent::GetRepeatCount(const MouseEvent& event) {
  int click_count = 1;
  if (last_click_event_) {
    if (event.type() == ui::ET_MOUSE_RELEASED) {
      if (event.changed_button_flags() ==
              last_click_event_->changed_button_flags()) {
        return last_click_event_->GetClickCount();
      } else {
        // If last_click_event_ has changed since this button was pressed
        // return a click count of 1.
        return click_count;
      }
    }
    // Return the prior click count and do not update |last_click_event_| when
    // re-processing a native event, or when proccesing a reposted event.
    if (event.time_stamp() == last_click_event_->time_stamp())
      return last_click_event_->GetClickCount();
    if (IsRepeatedClickEvent(*last_click_event_, event))
      click_count = last_click_event_->GetClickCount() + 1;
    delete last_click_event_;
  }
  last_click_event_ = new MouseEvent(event);
  if (click_count > 3)
    click_count = 3;
  last_click_event_->SetClickCount(click_count);
  return click_count;
}

void MouseEvent::ResetLastClickForTest() {
  if (last_click_event_) {
    delete last_click_event_;
    last_click_event_ = NULL;
  }
}

// static
MouseEvent* MouseEvent::last_click_event_ = NULL;

int MouseEvent::GetClickCount() const {
  if (type() != ET_MOUSE_PRESSED && type() != ET_MOUSE_RELEASED)
    return 0;

  if (flags() & EF_IS_TRIPLE_CLICK)
    return 3;
  else if (flags() & EF_IS_DOUBLE_CLICK)
    return 2;
  else
    return 1;
}

void MouseEvent::SetClickCount(int click_count) {
  if (type() != ET_MOUSE_PRESSED && type() != ET_MOUSE_RELEASED)
    return;

  DCHECK_LT(0, click_count);
  DCHECK_GE(3, click_count);

  int f = flags();
  switch (click_count) {
    case 1:
      f &= ~EF_IS_DOUBLE_CLICK;
      f &= ~EF_IS_TRIPLE_CLICK;
      break;
    case 2:
      f |= EF_IS_DOUBLE_CLICK;
      f &= ~EF_IS_TRIPLE_CLICK;
      break;
    case 3:
      f &= ~EF_IS_DOUBLE_CLICK;
      f |= EF_IS_TRIPLE_CLICK;
      break;
  }
  set_flags(f);
}

const PointerId MouseEvent::kMousePointerId =
    std::numeric_limits<PointerId>::max();

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent

MouseWheelEvent::MouseWheelEvent(const PlatformEvent& native_event)
    : MouseEvent(native_event), offset_(GetMouseWheelOffset(native_event)) {}

MouseWheelEvent::MouseWheelEvent(const ScrollEvent& scroll_event)
    : MouseEvent(scroll_event),
      offset_(gfx::ToRoundedInt(scroll_event.x_offset()),
              gfx::ToRoundedInt(scroll_event.y_offset())) {
  SetType(ET_MOUSEWHEEL);
}

MouseWheelEvent::MouseWheelEvent(const PointerEvent& pointer_event)
    : MouseEvent(pointer_event),
      offset_(pointer_event.pointer_details().offset.x(),
              pointer_event.pointer_details().offset.y()) {
  SetType(ET_MOUSEWHEEL);
}

MouseWheelEvent::MouseWheelEvent(const MouseEvent& mouse_event,
                                 int x_offset,
                                 int y_offset)
    : MouseEvent(mouse_event), offset_(x_offset, y_offset) {
  SetType(ET_MOUSEWHEEL);
}

MouseWheelEvent::MouseWheelEvent(const MouseWheelEvent& mouse_wheel_event)
    : MouseEvent(mouse_wheel_event),
      offset_(mouse_wheel_event.offset()) {
  DCHECK_EQ(ET_MOUSEWHEEL, type());
}

MouseWheelEvent::MouseWheelEvent(const gfx::Vector2d& offset,
                                 const gfx::Point& location,
                                 const gfx::Point& root_location,
                                 base::TimeTicks time_stamp,
                                 int flags,
                                 int changed_button_flags)
    : MouseEvent(ui::ET_UNKNOWN,
                 location,
                 root_location,
                 time_stamp,
                 flags,
                 changed_button_flags),
      offset_(offset) {
  // Set event type to ET_UNKNOWN initially in MouseEvent() to pass the
  // DCHECK for type to enforce that we use MouseWheelEvent() to create
  // a MouseWheelEvent.
  SetType(ui::ET_MOUSEWHEEL);
}

MouseWheelEvent::~MouseWheelEvent() = default;

#if defined(OS_WIN)
// This value matches windows WHEEL_DELTA.
// static
const int MouseWheelEvent::kWheelDelta = 120;
#else
// This value matches GTK+ wheel scroll amount.
const int MouseWheelEvent::kWheelDelta = 53;
#endif

////////////////////////////////////////////////////////////////////////////////
// TouchEvent

TouchEvent::TouchEvent(const PlatformEvent& native_event)
    : LocatedEvent(native_event),
      unique_event_id_(ui::GetNextTouchEventId()),
      may_cause_scrolling_(false),
      should_remove_native_touch_id_mapping_(false),
      hovering_(false),
      pointer_details_(GetTouchPointerDetailsFromNative(native_event)) {
  latency()->AddLatencyNumberWithTimestamp(
      INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, time_stamp(), 1);
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT);

  if (type() == ET_TOUCH_RELEASED || type() == ET_TOUCH_CANCELLED)
    should_remove_native_touch_id_mapping_ = true;
}

TouchEvent::TouchEvent(const PointerEvent& pointer_event)
    : LocatedEvent(pointer_event),
      unique_event_id_(ui::GetNextTouchEventId()),
      may_cause_scrolling_(false),
      should_remove_native_touch_id_mapping_(false),
      hovering_(false),
      pointer_details_(pointer_event.pointer_details()) {
  DCHECK(pointer_event.IsTouchPointerEvent() ||
         pointer_event.IsPenPointerEvent());
  switch (pointer_event.type()) {
    case ET_POINTER_DOWN:
      SetType(ET_TOUCH_PRESSED);
      break;

    case ET_POINTER_MOVED:
      SetType(ET_TOUCH_MOVED);
      break;

    case ET_POINTER_UP:
      SetType(ET_TOUCH_RELEASED);
      break;

    case ET_POINTER_CANCELLED:
      SetType(ET_TOUCH_CANCELLED);
      break;

    default:
      NOTREACHED();
  }
}

TouchEvent::TouchEvent(EventType type,
                       const gfx::Point& location,
                       base::TimeTicks time_stamp,
                       const PointerDetails& pointer_details,
                       int flags,
                       float angle)
    : LocatedEvent(type,
                   gfx::PointF(location),
                   gfx::PointF(location),
                   time_stamp,
                   flags),
      unique_event_id_(ui::GetNextTouchEventId()),
      may_cause_scrolling_(false),
      should_remove_native_touch_id_mapping_(false),
      hovering_(false),
      pointer_details_(pointer_details) {
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT);
  pointer_details_.twist = angle;
}

TouchEvent::TouchEvent(const TouchEvent& copy)
    : LocatedEvent(copy),
      unique_event_id_(copy.unique_event_id_),
      may_cause_scrolling_(copy.may_cause_scrolling_),
      should_remove_native_touch_id_mapping_(false),
      hovering_(copy.hovering_),
      pointer_details_(copy.pointer_details_) {
  // Copied events should not remove touch id mapping, as this either causes the
  // mapping to be lost before the initial event has finished dispatching, or
  // the copy to attempt to remove the mapping from a null |native_event_|.
}

TouchEvent::~TouchEvent() {
  // In ctor TouchEvent(native_event) we call GetTouchId() which in X11
  // platform setups the tracking_id to slot mapping. So in dtor here,
  // if this touch event is a release event, we clear the mapping accordingly.
  if (should_remove_native_touch_id_mapping_) {
    DCHECK(type() == ET_TOUCH_RELEASED || type() == ET_TOUCH_CANCELLED);
    if (type() == ET_TOUCH_RELEASED || type() == ET_TOUCH_CANCELLED)
      ClearTouchIdIfReleased(native_event());
  }
}

void TouchEvent::UpdateForRootTransform(
    const gfx::Transform& inverted_root_transform,
    const gfx::Transform& inverted_local_transform) {
  LocatedEvent::UpdateForRootTransform(inverted_root_transform,
                                       inverted_local_transform);
  gfx::DecomposedTransform decomp;
  bool success = gfx::DecomposeTransform(&decomp, inverted_root_transform);
  DCHECK(success);
  if (decomp.scale[0])
    pointer_details_.radius_x *= decomp.scale[0];
  if (decomp.scale[1])
    pointer_details_.radius_y *= decomp.scale[1];
}

void TouchEvent::DisableSynchronousHandling() {
  DispatcherApi dispatcher_api(this);
  dispatcher_api.set_result(
      static_cast<EventResult>(result() | ER_DISABLE_SYNC_HANDLING));
}

void TouchEvent::SetPointerDetailsForTest(
    const PointerDetails& pointer_details) {
  DCHECK_EQ(pointer_details_.id, pointer_details.id);
  pointer_details_ = pointer_details;
}

float TouchEvent::ComputeRotationAngle() const {
  float rotation_angle = pointer_details_.twist;
  while (rotation_angle < 0)
    rotation_angle += 180.f;
  while (rotation_angle >= 180)
    rotation_angle -= 180.f;
  return rotation_angle;
}

////////////////////////////////////////////////////////////////////////////////
// PointerEvent

bool PointerEvent::CanConvertFrom(const Event& event) {
  switch (event.type()) {
    case ET_MOUSE_PRESSED:
    case ET_MOUSE_DRAGGED:
    case ET_MOUSE_MOVED:
    case ET_MOUSE_ENTERED:
    case ET_MOUSE_EXITED:
    case ET_MOUSE_RELEASED:
    case ET_MOUSEWHEEL:
    case ET_MOUSE_CAPTURE_CHANGED:
    case ET_TOUCH_PRESSED:
    case ET_TOUCH_MOVED:
    case ET_TOUCH_RELEASED:
    case ET_TOUCH_CANCELLED:
      return true;
    default:
      return false;
  }
}

PointerEvent::PointerEvent(const PointerEvent& pointer_event)
    : LocatedEvent(pointer_event),
      changed_button_flags_(pointer_event.changed_button_flags()),
      details_(pointer_event.pointer_details()) {
  if (details_.pointer_type == EventPointerType::POINTER_TYPE_TOUCH)
    latency()->set_source_event_type(ui::SourceEventType::TOUCH);
  else if (pointer_event.type() == ET_POINTER_WHEEL_CHANGED)
    latency()->set_source_event_type(ui::SourceEventType::WHEEL);
  else if (details_.pointer_type == EventPointerType::POINTER_TYPE_MOUSE)
    latency()->set_source_event_type(ui::SourceEventType::MOUSE);
  else
    latency()->set_source_event_type(ui::SourceEventType::OTHER);
}

PointerEvent::PointerEvent(const MouseEvent& mouse_event)
    : LocatedEvent(mouse_event),
      changed_button_flags_(mouse_event.changed_button_flags()),
      details_(mouse_event.pointer_details()) {
  DCHECK(CanConvertFrom(mouse_event));
  switch (mouse_event.type()) {
    case ET_MOUSE_PRESSED:
      SetType(ET_POINTER_DOWN);
      latency()->set_source_event_type(ui::SourceEventType::MOUSE);
      break;

    case ET_MOUSE_DRAGGED:
    case ET_MOUSE_MOVED:
      SetType(ET_POINTER_MOVED);
      latency()->set_source_event_type(ui::SourceEventType::MOUSE);
      break;

    case ET_MOUSE_ENTERED:
      SetType(ET_POINTER_ENTERED);
      latency()->set_source_event_type(ui::SourceEventType::MOUSE);
      break;

    case ET_MOUSE_EXITED:
      SetType(ET_POINTER_EXITED);
      latency()->set_source_event_type(ui::SourceEventType::MOUSE);
      break;

    case ET_MOUSE_RELEASED:
      SetType(ET_POINTER_UP);
      latency()->set_source_event_type(ui::SourceEventType::MOUSE);
      break;

    case ET_MOUSEWHEEL:
      SetType(ET_POINTER_WHEEL_CHANGED);
      details_ = PointerDetails(EventPointerType::POINTER_TYPE_MOUSE,
                                mouse_event.AsMouseWheelEvent()->offset(),
                                mouse_event.pointer_details().id);
      latency()->set_source_event_type(ui::SourceEventType::WHEEL);
      break;

    case ET_MOUSE_CAPTURE_CHANGED:
      SetType(ET_POINTER_CAPTURE_CHANGED);
      break;

    default:
      NOTREACHED();
  }
}

PointerEvent::PointerEvent(const TouchEvent& touch_event)
    : LocatedEvent(touch_event),
      changed_button_flags_(0),
      details_(touch_event.pointer_details()) {
  DCHECK(CanConvertFrom(touch_event));
  switch (touch_event.type()) {
    case ET_TOUCH_PRESSED:
      SetType(ET_POINTER_DOWN);
      break;

    case ET_TOUCH_MOVED:
      SetType(ET_POINTER_MOVED);
      break;

    case ET_TOUCH_RELEASED:
      SetType(ET_POINTER_UP);
      break;

    case ET_TOUCH_CANCELLED:
      SetType(ET_POINTER_CANCELLED);
      break;

    default:
      NOTREACHED();
  }
  latency()->set_source_event_type(ui::SourceEventType::TOUCH);
}

PointerEvent::PointerEvent(EventType type,
                           const gfx::Point& location,
                           const gfx::Point& root_location,
                           int flags,
                           int changed_button_flags,
                           const PointerDetails& pointer_details,
                           base::TimeTicks time_stamp)
    : LocatedEvent(type,
                   gfx::PointF(location),
                   gfx::PointF(root_location),
                   time_stamp,
                   flags),
      changed_button_flags_(changed_button_flags),
      details_(pointer_details) {
  if (details_.pointer_type == EventPointerType::POINTER_TYPE_TOUCH)
    latency()->set_source_event_type(ui::SourceEventType::TOUCH);
  else if (type == ET_POINTER_WHEEL_CHANGED)
    latency()->set_source_event_type(ui::SourceEventType::WHEEL);
  else if (details_.pointer_type == EventPointerType::POINTER_TYPE_MOUSE)
    latency()->set_source_event_type(ui::SourceEventType::MOUSE);
  else
    latency()->set_source_event_type(ui::SourceEventType::OTHER);
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent

// static
KeyEvent* KeyEvent::last_key_event_ = nullptr;
#if defined(USE_X11)
KeyEvent* KeyEvent::last_ibus_key_event_ = nullptr;
#endif

// static
bool KeyEvent::IsRepeated(const KeyEvent& event) {
  // A safe guard in case if there were continous key pressed events that are
  // not auto repeat.
  const int kMaxAutoRepeatTimeMs = 2000;
  KeyEvent** last_key_event;
#if defined(USE_X11)
  // Use a different static variable for key events that have non standard
  // state masks as it may be reposted by an IME. IBUS-GTK uses this field
  // to detect the re-posted event for example. crbug.com/385873.
  last_key_event = X11EventHasNonStandardState(event.native_event())
                       ? &last_ibus_key_event_
                       : &last_key_event_;
#else
  last_key_event = &last_key_event_;
#endif
  if (event.is_char())
    return false;
  if (event.type() == ui::ET_KEY_RELEASED) {
    delete *last_key_event;
    *last_key_event = nullptr;
    return false;
  }

  CHECK_EQ(ui::ET_KEY_PRESSED, event.type());

  if (!(*last_key_event)) {
    *last_key_event = new KeyEvent(event);
    return false;
  } else if (event.time_stamp() == (*last_key_event)->time_stamp()) {
    // The KeyEvent is created from the same native event.
    return ((*last_key_event)->flags() & ui::EF_IS_REPEAT) != 0;
  }

  DCHECK(*last_key_event);
  bool is_repeat = false;

#if defined(OS_WIN)
  if (event.HasNativeEvent()) {
    // Bit 30 of lParam represents the "previous key state". If set, the key
    // was already down, therefore this is an auto-repeat.
    is_repeat = (event.native_event().lParam & 0x40000000) != 0;
  } else
#endif
  {
    // Note that this is only reach for non-native events under Windows.
    if (event.key_code() == (*last_key_event)->key_code() &&
        event.flags() == ((*last_key_event)->flags() & ~ui::EF_IS_REPEAT) &&
        (event.time_stamp() - (*last_key_event)->time_stamp())
                .InMilliseconds() < kMaxAutoRepeatTimeMs) {
      is_repeat = true;
    }
  }

  if (is_repeat) {
    (*last_key_event)->set_time_stamp(event.time_stamp());
    (*last_key_event)->set_flags((*last_key_event)->flags() | ui::EF_IS_REPEAT);
    return true;
  }

  delete *last_key_event;
  *last_key_event = new KeyEvent(event);

  return false;
}

KeyEvent::KeyEvent(const PlatformEvent& native_event)
    : KeyEvent(native_event, EventFlagsFromNative(native_event)) {}

KeyEvent::KeyEvent(const PlatformEvent& native_event, int event_flags)
    : Event(native_event, EventTypeFromNative(native_event), event_flags),
      key_code_(KeyboardCodeFromNative(native_event)),
      code_(CodeFromNative(native_event)),
      is_char_(IsCharFromNative(native_event)) {
  latency()->AddLatencyNumberWithTimestamp(
      INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, time_stamp(), 1);
  latency()->AddLatencyNumber(INPUT_EVENT_LATENCY_UI_COMPONENT);

  if (IsRepeated(*this))
    set_flags(flags() | ui::EF_IS_REPEAT);

#if defined(USE_X11)
  NormalizeFlags();
#endif
#if defined(OS_WIN)
  // Only Windows has native character events.
  if (is_char_) {
    key_ = DomKey::FromCharacter(native_event.wParam);
    set_flags(PlatformKeyMap::ReplaceControlAndAltWithAltGraph(flags()));
  } else {
    int adjusted_flags = flags();
    key_ = PlatformKeyMap::DomKeyFromKeyboardCode(key_code(), &adjusted_flags);
    set_flags(adjusted_flags);
  }
#endif
}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   int flags,
                   base::TimeTicks time_stamp)
    : Event(type,
            time_stamp == base::TimeTicks() ? EventTimeForNow() : time_stamp,
            flags),
      key_code_(key_code),
      code_(UsLayoutKeyboardCodeToDomCode(key_code)) {}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   DomCode code,
                   int flags)
    : Event(type, EventTimeForNow(), flags),
      key_code_(key_code),
      code_(code) {
}

KeyEvent::KeyEvent(EventType type,
                   KeyboardCode key_code,
                   DomCode code,
                   int flags,
                   DomKey key,
                   base::TimeTicks time_stamp)
    : Event(type, time_stamp, flags),
      key_code_(key_code),
      code_(code),
      key_(key) {}

KeyEvent::KeyEvent(base::char16 character,
                   KeyboardCode key_code,
                   DomCode code,
                   int flags,
                   base::TimeTicks time_stamp)
    : Event(ET_KEY_PRESSED,
            time_stamp == base::TimeTicks() ? EventTimeForNow() : time_stamp,
            flags),
      key_code_(key_code),
      code_(code),
      is_char_(true),
      key_(DomKey::FromCharacter(character)) {}

KeyEvent::KeyEvent(const KeyEvent& rhs)
    : Event(rhs),
      key_code_(rhs.key_code_),
      code_(rhs.code_),
      is_char_(rhs.is_char_),
      key_(rhs.key_),
      properties_(rhs.properties_
                      ? std::make_unique<Properties>(*rhs.properties_)
                      : nullptr) {}

KeyEvent& KeyEvent::operator=(const KeyEvent& rhs) {
  if (this != &rhs) {
    Event::operator=(rhs);
    key_code_ = rhs.key_code_;
    code_ = rhs.code_;
    key_ = rhs.key_;
    is_char_ = rhs.is_char_;
    if (rhs.properties_)
      properties_ = std::make_unique<Properties>(*rhs.properties_);
    else
      properties_.reset();
  }
  latency()->set_source_event_type(ui::SourceEventType::OTHER);
  return *this;
}

KeyEvent::~KeyEvent() {}

void KeyEvent::ApplyLayout() const {
  ui::DomCode code = code_;
  if (code == DomCode::NONE) {
    // Catch old code that tries to do layout without a physical key, and try
    // to recover using the KeyboardCode. Once key events are fully defined
    // on construction (see TODO in event.h) this will go away.
    VLOG(2) << "DomCode::NONE keycode=" << key_code_;
    code = UsLayoutKeyboardCodeToDomCode(key_code_);
    if (code == DomCode::NONE) {
      key_ = DomKey::UNIDENTIFIED;
      return;
    }
  }
  KeyboardCode dummy_key_code;
#if defined(OS_WIN)
// Native Windows character events always have is_char_ == true,
// so this is a synthetic or native keystroke event.
// Therefore, perform only the fallback action.
#elif defined(USE_X11)
  if (native_event()) {
    key_ = GetDomKeyFromXEvent(native_event());
    return;
  }
#elif defined(USE_OZONE)
  if (KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()->Lookup(
          code, flags(), &key_, &dummy_key_code)) {
    return;
  }
#else
  if (native_event()) {
    DCHECK(EventTypeFromNative(native_event()) == ET_KEY_PRESSED ||
           EventTypeFromNative(native_event()) == ET_KEY_RELEASED);
  }
#endif
  if (!DomCodeToUsLayoutDomKey(code, flags(), &key_, &dummy_key_code))
    key_ = DomKey::UNIDENTIFIED;
}

DomKey KeyEvent::GetDomKey() const {
  // Determination of key_ may be done lazily.
  if (key_ == DomKey::NONE)
    ApplyLayout();
  return key_;
}

base::char16 KeyEvent::GetCharacter() const {
  // Determination of key_ may be done lazily.
  if (key_ == DomKey::NONE)
    ApplyLayout();
  if (key_.IsCharacter()) {
    // Historically ui::KeyEvent has held only BMP characters.
    // Until this explicitly changes, require |key_| to hold a BMP character.
    DomKey::Base utf32_character = key_.ToCharacter();
    base::char16 ucs2_character = static_cast<base::char16>(utf32_character);
    DCHECK_EQ(static_cast<DomKey::Base>(ucs2_character), utf32_character);
    // Check if the control character is down. Note that ALTGR is represented
    // on Windows as CTRL|ALT, so we need to make sure that is not set.
    if ((flags() & (EF_ALTGR_DOWN | EF_CONTROL_DOWN)) == EF_CONTROL_DOWN) {
      // For a control character, key_ contains the corresponding printable
      // character. To preserve existing behaviour for now, return the control
      // character here; this will likely change -- see e.g. crbug.com/471488.
      if (ucs2_character >= 0x20 && ucs2_character <= 0x7E)
        return ucs2_character & 0x1F;
      if (ucs2_character == '\r')
        return '\n';
    }
    return ucs2_character;
  }
  return 0;
}

base::char16 KeyEvent::GetText() const {
  if ((flags() & EF_CONTROL_DOWN) != 0) {
    ui::DomKey key;
    ui::KeyboardCode key_code;
    if (DomCodeToControlCharacter(code_, flags(), &key, &key_code))
      return key.ToCharacter();
  }
  return GetUnmodifiedText();
}

base::char16 KeyEvent::GetUnmodifiedText() const {
  if (!is_char_ && (key_code_ == VKEY_RETURN))
    return '\r';
  return GetCharacter();
}

bool KeyEvent::IsUnicodeKeyCode() const {
#if defined(OS_WIN)
  if (!IsAltDown())
    return false;
  const int key = key_code();
  if (key >= VKEY_NUMPAD0 && key <= VKEY_NUMPAD9)
    return true;
  // Check whether the user is using the numeric keypad with num-lock off.
  // In that case, EF_EXTENDED will not be set; if it is set, the key event
  // originated from the relevant non-numpad dedicated key, e.g. [Insert].
  return (!(flags() & EF_IS_EXTENDED_KEY) &&
          (key == VKEY_INSERT || key == VKEY_END  || key == VKEY_DOWN ||
           key == VKEY_NEXT   || key == VKEY_LEFT || key == VKEY_CLEAR ||
           key == VKEY_RIGHT  || key == VKEY_HOME || key == VKEY_UP ||
           key == VKEY_PRIOR));
#else
  return false;
#endif
}

void KeyEvent::NormalizeFlags() {
  int mask = 0;
  switch (key_code()) {
    case VKEY_CONTROL:
      mask = EF_CONTROL_DOWN;
      break;
    case VKEY_SHIFT:
      mask = EF_SHIFT_DOWN;
      break;
    case VKEY_MENU:
      mask = EF_ALT_DOWN;
      break;
    default:
      return;
  }
  if (type() == ET_KEY_PRESSED)
    set_flags(flags() | mask);
  else
    set_flags(flags() & ~mask);
}

void KeyEvent::SetProperties(const Properties& properties) {
  properties_ = std::make_unique<Properties>(properties);
}

KeyboardCode KeyEvent::GetLocatedWindowsKeyboardCode() const {
  return NonLocatedToLocatedKeyboardCode(key_code_, code_);
}

uint16_t KeyEvent::GetConflatedWindowsKeyCode() const {
  if (is_char_)
    return key_.ToCharacter();
  return key_code_;
}

std::string KeyEvent::GetCodeString() const {
  return KeycodeConverter::DomCodeToCodeString(code_);
}

////////////////////////////////////////////////////////////////////////////////
// ScrollEvent

ScrollEvent::ScrollEvent(const PlatformEvent& native_event)
    : MouseEvent(native_event),
      x_offset_(0.0f),
      y_offset_(0.0f),
      x_offset_ordinal_(0.0f),
      y_offset_ordinal_(0.0f),
      finger_count_(0),
      momentum_phase_(EventMomentumPhase::NONE),
      scroll_event_phase_(ScrollEventPhase::kNone) {
  if (type() == ET_SCROLL) {
    GetScrollOffsets(native_event, &x_offset_, &y_offset_, &x_offset_ordinal_,
                     &y_offset_ordinal_, &finger_count_, &momentum_phase_);
  } else if (type() == ET_SCROLL_FLING_START ||
             type() == ET_SCROLL_FLING_CANCEL) {
    GetFlingData(native_event,
                 &x_offset_, &y_offset_,
                 &x_offset_ordinal_, &y_offset_ordinal_,
                 NULL);
  } else {
    NOTREACHED() << "Unexpected event type " << type()
        << " when constructing a ScrollEvent.";
  }
  if (IsScrollEvent())
    latency()->set_source_event_type(ui::SourceEventType::WHEEL);
  else
    latency()->set_source_event_type(ui::SourceEventType::TOUCH);
}

ScrollEvent::ScrollEvent(EventType type,
                         const gfx::Point& location,
                         base::TimeTicks time_stamp,
                         int flags,
                         float x_offset,
                         float y_offset,
                         float x_offset_ordinal,
                         float y_offset_ordinal,
                         int finger_count,
                         EventMomentumPhase momentum_phase,
                         ScrollEventPhase scroll_event_phase)
    : MouseEvent(type, location, location, time_stamp, flags, 0),
      x_offset_(x_offset),
      y_offset_(y_offset),
      x_offset_ordinal_(x_offset_ordinal),
      y_offset_ordinal_(y_offset_ordinal),
      finger_count_(finger_count),
      momentum_phase_(momentum_phase),
      scroll_event_phase_(scroll_event_phase) {
  CHECK(IsScrollEvent());
  latency()->set_source_event_type(ui::SourceEventType::WHEEL);
}

ScrollEvent::ScrollEvent(const ScrollEvent& other) = default;

ScrollEvent::~ScrollEvent() = default;

void ScrollEvent::Scale(const float factor) {
  x_offset_ *= factor;
  y_offset_ *= factor;
  x_offset_ordinal_ *= factor;
  y_offset_ordinal_ *= factor;
}

////////////////////////////////////////////////////////////////////////////////
// GestureEvent

GestureEvent::GestureEvent(float x,
                           float y,
                           int flags,
                           base::TimeTicks time_stamp,
                           const GestureEventDetails& details,
                           uint32_t unique_touch_event_id)
    : LocatedEvent(details.type(),
                   gfx::PointF(x, y),
                   gfx::PointF(x, y),
                   time_stamp,
                   flags | EF_FROM_TOUCH),
      details_(details),
      unique_touch_event_id_(unique_touch_event_id) {
  latency()->set_source_event_type(ui::SourceEventType::TOUCH);
  // TODO(crbug.com/868056) Other touchpad gesture should report as TOUCHPAD.
  if (IsPinchEvent() &&
      details.device_type() == ui::GestureDeviceType::DEVICE_TOUCHPAD) {
    latency()->set_source_event_type(ui::SourceEventType::TOUCHPAD);
  }
}

GestureEvent::GestureEvent(const GestureEvent& other) = default;

GestureEvent::~GestureEvent() = default;

}  // namespace ui
