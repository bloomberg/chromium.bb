// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "ui/events/blink/blink_event_util.h"

#include <stddef.h>

#include <algorithm>
#include <bitset>
#include <cmath>
#include <limits>

#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/transform.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using std::numeric_limits;

namespace ui {
namespace {

const int kInvalidTouchIndex = -1;

WebInputEvent::Type ToWebTouchEventType(MotionEvent::Action action) {
  switch (action) {
    case MotionEvent::ACTION_DOWN:
      return WebInputEvent::TouchStart;
    case MotionEvent::ACTION_MOVE:
      return WebInputEvent::TouchMove;
    case MotionEvent::ACTION_UP:
      return WebInputEvent::TouchEnd;
    case MotionEvent::ACTION_CANCEL:
      return WebInputEvent::TouchCancel;
    case MotionEvent::ACTION_POINTER_DOWN:
      return WebInputEvent::TouchStart;
    case MotionEvent::ACTION_POINTER_UP:
      return WebInputEvent::TouchEnd;
    case MotionEvent::ACTION_NONE:
    case MotionEvent::ACTION_HOVER_ENTER:
    case MotionEvent::ACTION_HOVER_EXIT:
    case MotionEvent::ACTION_HOVER_MOVE:
    case MotionEvent::ACTION_BUTTON_PRESS:
    case MotionEvent::ACTION_BUTTON_RELEASE:
      break;
  }
  NOTREACHED() << "Invalid MotionEvent::Action = " << action;
  return WebInputEvent::Undefined;
}

// Note that the action index is meaningful only in the context of
// |ACTION_POINTER_UP| and |ACTION_POINTER_DOWN|; other actions map directly to
// WebTouchPoint::State.
WebTouchPoint::State ToWebTouchPointState(const MotionEvent& event,
                                          size_t pointer_index) {
  switch (event.GetAction()) {
    case MotionEvent::ACTION_DOWN:
      return WebTouchPoint::StatePressed;
    case MotionEvent::ACTION_MOVE:
      return WebTouchPoint::StateMoved;
    case MotionEvent::ACTION_UP:
      return WebTouchPoint::StateReleased;
    case MotionEvent::ACTION_CANCEL:
      return WebTouchPoint::StateCancelled;
    case MotionEvent::ACTION_POINTER_DOWN:
      return static_cast<int>(pointer_index) == event.GetActionIndex()
                 ? WebTouchPoint::StatePressed
                 : WebTouchPoint::StateStationary;
    case MotionEvent::ACTION_POINTER_UP:
      return static_cast<int>(pointer_index) == event.GetActionIndex()
                 ? WebTouchPoint::StateReleased
                 : WebTouchPoint::StateStationary;
    case MotionEvent::ACTION_NONE:
    case MotionEvent::ACTION_HOVER_ENTER:
    case MotionEvent::ACTION_HOVER_EXIT:
    case MotionEvent::ACTION_HOVER_MOVE:
    case MotionEvent::ACTION_BUTTON_PRESS:
    case MotionEvent::ACTION_BUTTON_RELEASE:
      break;
  }
  NOTREACHED() << "Invalid MotionEvent::Action.";
  return WebTouchPoint::StateUndefined;
}

WebPointerProperties::PointerType ToWebPointerType(int tool_type) {
  switch (static_cast<MotionEvent::ToolType>(tool_type)) {
    case MotionEvent::TOOL_TYPE_UNKNOWN:
      return WebPointerProperties::PointerType::Unknown;
    case MotionEvent::TOOL_TYPE_FINGER:
      return WebPointerProperties::PointerType::Touch;
    case MotionEvent::TOOL_TYPE_STYLUS:
      return WebPointerProperties::PointerType::Pen;
    case MotionEvent::TOOL_TYPE_MOUSE:
      return WebPointerProperties::PointerType::Mouse;
    case MotionEvent::TOOL_TYPE_ERASER:
      return WebPointerProperties::PointerType::Eraser;
  }
  NOTREACHED() << "Invalid MotionEvent::ToolType = " << tool_type;
  return WebPointerProperties::PointerType::Unknown;
}

WebPointerProperties::Button ToWebPointerButton(int android_button_state) {
    if (android_button_state & MotionEvent::BUTTON_PRIMARY)
        return WebPointerProperties::Button::Left;
    else if (android_button_state & MotionEvent::BUTTON_SECONDARY)
        return WebPointerProperties::Button::Right;
    else if (android_button_state & MotionEvent::BUTTON_TERTIARY)
        return WebPointerProperties::Button::Middle;
    else if (android_button_state & MotionEvent::BUTTON_BACK)
        return WebPointerProperties::Button::X1;
    else if (android_button_state & MotionEvent::BUTTON_FORWARD)
        return WebPointerProperties::Button::X2;
    else if (android_button_state & MotionEvent::BUTTON_STYLUS_PRIMARY)
        return WebPointerProperties::Button::Left;
    else if (android_button_state & MotionEvent::BUTTON_STYLUS_SECONDARY)
        return WebPointerProperties::Button::Right;
    else
        return WebPointerProperties::Button::NoButton;
}

WebTouchPoint CreateWebTouchPoint(const MotionEvent& event,
                                  size_t pointer_index) {
  WebTouchPoint touch;

  SetWebPointerPropertiesFromMotionEventData(
      touch, event.GetPointerId(pointer_index),
      event.GetPressure(pointer_index), event.GetOrientation(pointer_index),
      event.GetTilt(pointer_index), 0 /* no button changed */,
      event.GetToolType(pointer_index));

  touch.state = ToWebTouchPointState(event, pointer_index);
  touch.position.x = event.GetX(pointer_index);
  touch.position.y = event.GetY(pointer_index);
  touch.screenPosition.x = event.GetRawX(pointer_index);
  touch.screenPosition.y = event.GetRawY(pointer_index);

  // A note on touch ellipse specifications:
  //
  // Android MotionEvent provides the major and minor axes of the touch ellipse,
  // as well as the orientation of the major axis clockwise from vertical, in
  // radians. See:
  // http://developer.android.com/reference/android/view/MotionEvent.html
  //
  // The proposed extension to W3C Touch Events specifies the touch ellipse
  // using two radii along x- & y-axes and a positive acute rotation angle in
  // degrees. See:
  // http://dvcs.w3.org/hg/webevents/raw-file/default/touchevents.html

  float major_radius = event.GetTouchMajor(pointer_index) / 2.f;
  float minor_radius = event.GetTouchMinor(pointer_index) / 2.f;
  float orientation_deg = event.GetOrientation(pointer_index) * 180.f / M_PI;

  DCHECK_GE(major_radius, 0);
  DCHECK_GE(minor_radius, 0);
  DCHECK_GE(major_radius, minor_radius);
  // Orientation lies in [-180, 180] for a stylus, and [-90, 90] for other
  // touchscreen inputs. There are exceptions on Android when a device is
  // rotated, yielding touch orientations in the range of [-180, 180].
  // Regardless, normalise to [-90, 90), allowing a small tolerance to account
  // for floating point conversion.
  // TODO(e_hakkinen): Also pass unaltered stylus orientation, avoiding loss of
  // quadrant information, see crbug.com/493728.
  DCHECK_GT(orientation_deg, -180.01f);
  DCHECK_LT(orientation_deg, 180.01f);
  if (orientation_deg >= 90.f)
    orientation_deg -= 180.f;
  else if (orientation_deg < -90.f)
    orientation_deg += 180.f;
  if (orientation_deg >= 0) {
    // The case orientation_deg == 0 is handled here on purpose: although the
    // 'else' block is equivalent in this case, we want to pass the 0 value
    // unchanged (and 0 is the default value for many devices that don't
    // report elliptical touches).
    touch.radiusX = minor_radius;
    touch.radiusY = major_radius;
    touch.rotationAngle = orientation_deg;
  } else {
    touch.radiusX = major_radius;
    touch.radiusY = minor_radius;
    touch.rotationAngle = orientation_deg + 90;
  }

  return touch;
}

float GetUnacceleratedDelta(float accelerated_delta, float acceleration_ratio) {
  return accelerated_delta * acceleration_ratio;
}

float GetAccelerationRatio(float accelerated_delta, float unaccelerated_delta) {
  if (unaccelerated_delta == 0.f || accelerated_delta == 0.f)
    return 1.f;
  return unaccelerated_delta / accelerated_delta;
}

// Returns |kInvalidTouchIndex| iff |event| lacks a touch with an ID of |id|.
int GetIndexOfTouchID(const WebTouchEvent& event, int id) {
  for (unsigned i = 0; i < event.touchesLength; ++i) {
    if (event.touches[i].id == id)
      return i;
  }
  return kInvalidTouchIndex;
}

WebInputEvent::DispatchType MergeDispatchTypes(
    WebInputEvent::DispatchType type_1,
    WebInputEvent::DispatchType type_2) {
  static_assert(WebInputEvent::DispatchType::Blocking <
                    WebInputEvent::DispatchType::EventNonBlocking,
                "Enum not ordered correctly");
  static_assert(WebInputEvent::DispatchType::EventNonBlocking <
                    WebInputEvent::DispatchType::ListenersNonBlockingPassive,
                "Enum not ordered correctly");
  static_assert(
      WebInputEvent::DispatchType::ListenersNonBlockingPassive <
          WebInputEvent::DispatchType::ListenersForcedNonBlockingDueToFling,
      "Enum not ordered correctly");
  return static_cast<WebInputEvent::DispatchType>(
      std::min(static_cast<int>(type_1), static_cast<int>(type_2)));
}

bool CanCoalesce(const WebMouseEvent& event_to_coalesce,
                 const WebMouseEvent& event) {
  return event.type() == event_to_coalesce.type() &&
         event.type() == WebInputEvent::MouseMove;
}

void Coalesce(const WebMouseEvent& event_to_coalesce, WebMouseEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  // Accumulate movement deltas.
  int x = event->movementX;
  int y = event->movementY;
  *event = event_to_coalesce;
  event->movementX += x;
  event->movementY += y;
}

bool CanCoalesce(const WebMouseWheelEvent& event_to_coalesce,
                 const WebMouseWheelEvent& event) {
  return event.modifiers() == event_to_coalesce.modifiers() &&
         event.scrollByPage == event_to_coalesce.scrollByPage &&
         event.phase == event_to_coalesce.phase &&
         event.momentumPhase == event_to_coalesce.momentumPhase &&
         event.resendingPluginId == event_to_coalesce.resendingPluginId &&
         event.hasPreciseScrollingDeltas ==
             event_to_coalesce.hasPreciseScrollingDeltas;
}

void Coalesce(const WebMouseWheelEvent& event_to_coalesce,
              WebMouseWheelEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  float unaccelerated_x =
      GetUnacceleratedDelta(event->deltaX, event->accelerationRatioX) +
      GetUnacceleratedDelta(event_to_coalesce.deltaX,
                            event_to_coalesce.accelerationRatioX);
  float unaccelerated_y =
      GetUnacceleratedDelta(event->deltaY, event->accelerationRatioY) +
      GetUnacceleratedDelta(event_to_coalesce.deltaY,
                            event_to_coalesce.accelerationRatioY);
  float old_deltaX = event->deltaX;
  float old_deltaY = event->deltaY;
  float old_wheelTicksX = event->wheelTicksX;
  float old_wheelTicksY = event->wheelTicksY;
  float old_movementX = event->movementX;
  float old_movementY = event->movementY;
  *event = event_to_coalesce;
  event->deltaX += old_deltaX;
  event->deltaY += old_deltaY;
  event->wheelTicksX += old_wheelTicksX;
  event->wheelTicksY += old_wheelTicksY;
  event->movementX += old_movementX;
  event->movementY += old_movementY;
  event->accelerationRatioX =
      GetAccelerationRatio(event->deltaX, unaccelerated_x);
  event->accelerationRatioY =
      GetAccelerationRatio(event->deltaY, unaccelerated_y);
}

bool CanCoalesce(const WebTouchEvent& event_to_coalesce,
                 const WebTouchEvent& event) {
  if (event.type() != event_to_coalesce.type() ||
      event.type() != WebInputEvent::TouchMove ||
      event.modifiers() != event_to_coalesce.modifiers() ||
      event.touchesLength != event_to_coalesce.touchesLength ||
      event.touchesLength > WebTouchEvent::kTouchesLengthCap)
    return false;

  static_assert(WebTouchEvent::kTouchesLengthCap <= sizeof(int32_t) * 8U,
                "suboptimal kTouchesLengthCap size");
  // Ensure that we have a 1-to-1 mapping of pointer ids between touches.
  std::bitset<WebTouchEvent::kTouchesLengthCap> unmatched_event_touches(
      (1 << event.touchesLength) - 1);
  for (unsigned i = 0; i < event_to_coalesce.touchesLength; ++i) {
    int event_touch_index =
        GetIndexOfTouchID(event, event_to_coalesce.touches[i].id);
    if (event_touch_index == kInvalidTouchIndex)
      return false;
    if (!unmatched_event_touches[event_touch_index])
      return false;
    unmatched_event_touches[event_touch_index] = false;
  }
  return unmatched_event_touches.none();
}

void Coalesce(const WebTouchEvent& event_to_coalesce, WebTouchEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  // The WebTouchPoints include absolute position information. So it is
  // sufficient to simply replace the previous event with the new event->
  // However, it is necessary to make sure that all the points have the
  // correct state, i.e. the touch-points that moved in the last event, but
  // didn't change in the current event, will have Stationary state. It is
  // necessary to change them back to Moved state.
  WebTouchEvent old_event = *event;
  *event = event_to_coalesce;
  for (unsigned i = 0; i < event->touchesLength; ++i) {
    int i_old = GetIndexOfTouchID(old_event, event->touches[i].id);
    if (old_event.touches[i_old].state == blink::WebTouchPoint::StateMoved) {
      event->touches[i].state = blink::WebTouchPoint::StateMoved;
      event->touches[i].movementX += old_event.touches[i_old].movementX;
      event->touches[i].movementY += old_event.touches[i_old].movementY;
    }
  }
  event->movedBeyondSlopRegion |= old_event.movedBeyondSlopRegion;
  event->dispatchType = MergeDispatchTypes(old_event.dispatchType,
                                           event_to_coalesce.dispatchType);
}

bool CanCoalesce(const WebGestureEvent& event_to_coalesce,
                 const WebGestureEvent& event) {
  if (event.type() != event_to_coalesce.type() ||
      event.resendingPluginId != event_to_coalesce.resendingPluginId ||
      event.sourceDevice != event_to_coalesce.sourceDevice ||
      event.modifiers() != event_to_coalesce.modifiers())
    return false;

  if (event.type() == WebInputEvent::GestureScrollUpdate)
    return true;

  // GesturePinchUpdate scales can be combined only if they share a focal point,
  // e.g., with double-tap drag zoom.
  if (event.type() == WebInputEvent::GesturePinchUpdate &&
      event.x == event_to_coalesce.x && event.y == event_to_coalesce.y)
    return true;

  return false;
}

void Coalesce(const WebGestureEvent& event_to_coalesce,
              WebGestureEvent* event) {
  DCHECK(CanCoalesce(event_to_coalesce, *event));
  if (event->type() == WebInputEvent::GestureScrollUpdate) {
    event->data.scrollUpdate.deltaX +=
        event_to_coalesce.data.scrollUpdate.deltaX;
    event->data.scrollUpdate.deltaY +=
        event_to_coalesce.data.scrollUpdate.deltaY;
    DCHECK_EQ(
        event->data.scrollUpdate.previousUpdateInSequencePrevented,
        event_to_coalesce.data.scrollUpdate.previousUpdateInSequencePrevented);
  } else if (event->type() == WebInputEvent::GesturePinchUpdate) {
    event->data.pinchUpdate.scale *= event_to_coalesce.data.pinchUpdate.scale;
    // Ensure the scale remains bounded above 0 and below Infinity so that
    // we can reliably perform operations like log on the values.
    if (event->data.pinchUpdate.scale < numeric_limits<float>::min())
      event->data.pinchUpdate.scale = numeric_limits<float>::min();
    else if (event->data.pinchUpdate.scale > numeric_limits<float>::max())
      event->data.pinchUpdate.scale = numeric_limits<float>::max();
  }
}

// Returns the transform matrix corresponding to the gesture event.
gfx::Transform GetTransformForEvent(const WebGestureEvent& gesture_event) {
  gfx::Transform gesture_transform;
  if (gesture_event.type() == WebInputEvent::GestureScrollUpdate) {
    gesture_transform.Translate(gesture_event.data.scrollUpdate.deltaX,
                                gesture_event.data.scrollUpdate.deltaY);
  } else if (gesture_event.type() == WebInputEvent::GesturePinchUpdate) {
    float scale = gesture_event.data.pinchUpdate.scale;
    gesture_transform.Translate(-gesture_event.x, -gesture_event.y);
    gesture_transform.Scale(scale, scale);
    gesture_transform.Translate(gesture_event.x, gesture_event.y);
  } else {
    NOTREACHED() << "Invalid event type for transform retrieval: "
                 << WebInputEvent::GetName(gesture_event.type());
  }
  return gesture_transform;
}

}  // namespace

bool CanCoalesce(const blink::WebInputEvent& event_to_coalesce,
                 const blink::WebInputEvent& event) {
  if (blink::WebInputEvent::isGestureEventType(event_to_coalesce.type()) &&
      blink::WebInputEvent::isGestureEventType(event.type())) {
    return CanCoalesce(
        static_cast<const blink::WebGestureEvent&>(event_to_coalesce),
        static_cast<const blink::WebGestureEvent&>(event));
  }
  if (blink::WebInputEvent::isMouseEventType(event_to_coalesce.type()) &&
      blink::WebInputEvent::isMouseEventType(event.type())) {
    return CanCoalesce(
        static_cast<const blink::WebMouseEvent&>(event_to_coalesce),
        static_cast<const blink::WebMouseEvent&>(event));
  }
  if (blink::WebInputEvent::isTouchEventType(event_to_coalesce.type()) &&
      blink::WebInputEvent::isTouchEventType(event.type())) {
    return CanCoalesce(
        static_cast<const blink::WebTouchEvent&>(event_to_coalesce),
        static_cast<const blink::WebTouchEvent&>(event));
  }
  if (event_to_coalesce.type() == blink::WebInputEvent::MouseWheel &&
      event.type() == blink::WebInputEvent::MouseWheel) {
    return CanCoalesce(
        static_cast<const blink::WebMouseWheelEvent&>(event_to_coalesce),
        static_cast<const blink::WebMouseWheelEvent&>(event));
  }
  return false;
}

void Coalesce(const blink::WebInputEvent& event_to_coalesce,
              blink::WebInputEvent* event) {
  if (blink::WebInputEvent::isGestureEventType(event_to_coalesce.type()) &&
      blink::WebInputEvent::isGestureEventType(event->type())) {
    Coalesce(static_cast<const blink::WebGestureEvent&>(event_to_coalesce),
             static_cast<blink::WebGestureEvent*>(event));
    return;
  }
  if (blink::WebInputEvent::isMouseEventType(event_to_coalesce.type()) &&
      blink::WebInputEvent::isMouseEventType(event->type())) {
    Coalesce(static_cast<const blink::WebMouseEvent&>(event_to_coalesce),
             static_cast<blink::WebMouseEvent*>(event));
    return;
  }
  if (blink::WebInputEvent::isTouchEventType(event_to_coalesce.type()) &&
      blink::WebInputEvent::isTouchEventType(event->type())) {
    Coalesce(static_cast<const blink::WebTouchEvent&>(event_to_coalesce),
             static_cast<blink::WebTouchEvent*>(event));
    return;
  }
  if (event_to_coalesce.type() == blink::WebInputEvent::MouseWheel &&
      event->type() == blink::WebInputEvent::MouseWheel) {
    Coalesce(static_cast<const blink::WebMouseWheelEvent&>(event_to_coalesce),
             static_cast<blink::WebMouseWheelEvent*>(event));
  }
}

// Whether |event_in_queue| is GesturePinchUpdate or GestureScrollUpdate and
// has the same modifiers/source as the new scroll/pinch event. Compatible
// scroll and pinch event pairs can be logically coalesced.
bool IsCompatibleScrollorPinch(const WebGestureEvent& new_event,
                               const WebGestureEvent& event_in_queue) {
  DCHECK(new_event.type() == WebInputEvent::GestureScrollUpdate ||
         new_event.type() == WebInputEvent::GesturePinchUpdate)
      << "Invalid event type for pinch/scroll coalescing: "
      << WebInputEvent::GetName(new_event.type());
  DLOG_IF(WARNING,
          new_event.timeStampSeconds() < event_in_queue.timeStampSeconds())
      << "Event time not monotonic?\n";
  return (event_in_queue.type() == WebInputEvent::GestureScrollUpdate ||
          event_in_queue.type() == WebInputEvent::GesturePinchUpdate) &&
         event_in_queue.modifiers() == new_event.modifiers() &&
         event_in_queue.sourceDevice == new_event.sourceDevice;
}

std::pair<WebGestureEvent, WebGestureEvent> CoalesceScrollAndPinch(
    const WebGestureEvent* second_last_event,
    const WebGestureEvent& last_event,
    const WebGestureEvent& new_event) {
  DCHECK(!CanCoalesce(new_event, last_event))
      << "New event can be coalesced with the last event in queue directly.";
  DCHECK(IsContinuousGestureEvent(new_event.type()));
  DCHECK(IsCompatibleScrollorPinch(new_event, last_event));
  DCHECK(!second_last_event ||
         IsCompatibleScrollorPinch(new_event, *second_last_event));

  WebGestureEvent scroll_event(WebInputEvent::GestureScrollUpdate,
                               new_event.modifiers(),
                               new_event.timeStampSeconds());
  WebGestureEvent pinch_event;
  scroll_event.sourceDevice = new_event.sourceDevice;
  pinch_event = scroll_event;
  pinch_event.setType(WebInputEvent::GesturePinchUpdate);
  pinch_event.x = new_event.type() == WebInputEvent::GesturePinchUpdate
                      ? new_event.x
                      : last_event.x;
  pinch_event.y = new_event.type() == WebInputEvent::GesturePinchUpdate
                      ? new_event.y
                      : last_event.y;

  gfx::Transform combined_scroll_pinch = GetTransformForEvent(last_event);
  if (second_last_event) {
    combined_scroll_pinch.PreconcatTransform(
        GetTransformForEvent(*second_last_event));
  }
  combined_scroll_pinch.ConcatTransform(GetTransformForEvent(new_event));

  float combined_scale =
      SkMScalarToFloat(combined_scroll_pinch.matrix().get(0, 0));
  float combined_scroll_pinch_x =
      SkMScalarToFloat(combined_scroll_pinch.matrix().get(0, 3));
  float combined_scroll_pinch_y =
      SkMScalarToFloat(combined_scroll_pinch.matrix().get(1, 3));
  scroll_event.data.scrollUpdate.deltaX =
      (combined_scroll_pinch_x + pinch_event.x) / combined_scale -
      pinch_event.x;
  scroll_event.data.scrollUpdate.deltaY =
      (combined_scroll_pinch_y + pinch_event.y) / combined_scale -
      pinch_event.y;
  pinch_event.data.pinchUpdate.scale = combined_scale;

  return std::make_pair(scroll_event, pinch_event);
}

blink::WebTouchEvent CreateWebTouchEventFromMotionEvent(
    const MotionEvent& event,
    bool moved_beyond_slop_region) {
  static_assert(static_cast<int>(MotionEvent::MAX_TOUCH_POINT_COUNT) ==
                    static_cast<int>(blink::WebTouchEvent::kTouchesLengthCap),
                "inconsistent maximum number of active touch points");

  blink::WebTouchEvent result(
      ToWebTouchEventType(event.GetAction()),
      EventFlagsToWebEventModifiers(event.GetFlags()),
      ui::EventTimeStampToSeconds(event.GetEventTime()));
  result.dispatchType = result.type() == WebInputEvent::TouchCancel
                            ? WebInputEvent::EventNonBlocking
                            : WebInputEvent::Blocking;
  result.movedBeyondSlopRegion = moved_beyond_slop_region;

  // TODO(mustaq): MotionEvent flags seems unrelated, should use
  // metaState instead?

  DCHECK_NE(event.GetUniqueEventId(), 0U);
  result.uniqueTouchEventId = event.GetUniqueEventId();
  result.touchesLength =
      std::min(static_cast<unsigned>(event.GetPointerCount()),
               static_cast<unsigned>(WebTouchEvent::kTouchesLengthCap));
  DCHECK_GT(result.touchesLength, 0U);

  for (size_t i = 0; i < result.touchesLength; ++i)
    result.touches[i] = CreateWebTouchPoint(event, i);

  return result;
}

int EventFlagsToWebEventModifiers(int flags) {
  int modifiers = 0;

  if (flags & EF_SHIFT_DOWN)
    modifiers |= blink::WebInputEvent::ShiftKey;
  if (flags & EF_CONTROL_DOWN)
    modifiers |= blink::WebInputEvent::ControlKey;
  if (flags & EF_ALT_DOWN)
    modifiers |= blink::WebInputEvent::AltKey;
  if (flags & EF_COMMAND_DOWN)
    modifiers |= blink::WebInputEvent::MetaKey;
  if (flags & EF_ALTGR_DOWN)
    modifiers |= blink::WebInputEvent::AltGrKey;
  if (flags & EF_NUM_LOCK_ON)
    modifiers |= blink::WebInputEvent::NumLockOn;
  if (flags & EF_CAPS_LOCK_ON)
    modifiers |= blink::WebInputEvent::CapsLockOn;
  if (flags & EF_SCROLL_LOCK_ON)
    modifiers |= blink::WebInputEvent::ScrollLockOn;
  if (flags & EF_LEFT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::LeftButtonDown;
  if (flags & EF_MIDDLE_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::MiddleButtonDown;
  if (flags & EF_RIGHT_MOUSE_BUTTON)
    modifiers |= blink::WebInputEvent::RightButtonDown;
  if (flags & EF_IS_REPEAT)
    modifiers |= blink::WebInputEvent::IsAutoRepeat;
  if (flags & EF_TOUCH_ACCESSIBILITY)
    modifiers |= blink::WebInputEvent::IsTouchAccessibility;

  return modifiers;
}

WebGestureEvent CreateWebGestureEvent(const GestureEventDetails& details,
                                      base::TimeTicks timestamp,
                                      const gfx::PointF& location,
                                      const gfx::PointF& raw_location,
                                      int flags,
                                      uint32_t unique_touch_event_id) {
  WebGestureEvent gesture(WebInputEvent::Undefined,
                          EventFlagsToWebEventModifiers(flags),
                          ui::EventTimeStampToSeconds(timestamp));
  gesture.x = gfx::ToFlooredInt(location.x());
  gesture.y = gfx::ToFlooredInt(location.y());
  gesture.globalX = gfx::ToFlooredInt(raw_location.x());
  gesture.globalY = gfx::ToFlooredInt(raw_location.y());

  switch (details.device_type()) {
    case GestureDeviceType::DEVICE_TOUCHSCREEN:
      gesture.sourceDevice = blink::WebGestureDeviceTouchscreen;
      break;
    case GestureDeviceType::DEVICE_TOUCHPAD:
      gesture.sourceDevice = blink::WebGestureDeviceTouchpad;
      break;
    case GestureDeviceType::DEVICE_UNKNOWN:
      NOTREACHED() << "Unknown device type is not allowed";
      gesture.sourceDevice = blink::WebGestureDeviceUninitialized;
      break;
  }

  gesture.uniqueTouchEventId = unique_touch_event_id;

  switch (details.type()) {
    case ET_GESTURE_SHOW_PRESS:
      gesture.setType(WebInputEvent::GestureShowPress);
      gesture.data.showPress.width = details.bounding_box_f().width();
      gesture.data.showPress.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_DOUBLE_TAP:
      gesture.setType(WebInputEvent::GestureDoubleTap);
      DCHECK_EQ(1, details.tap_count());
      gesture.data.tap.tapCount = details.tap_count();
      gesture.data.tap.width = details.bounding_box_f().width();
      gesture.data.tap.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_TAP:
      gesture.setType(WebInputEvent::GestureTap);
      DCHECK_GE(details.tap_count(), 1);
      gesture.data.tap.tapCount = details.tap_count();
      gesture.data.tap.width = details.bounding_box_f().width();
      gesture.data.tap.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_TAP_UNCONFIRMED:
      gesture.setType(WebInputEvent::GestureTapUnconfirmed);
      DCHECK_EQ(1, details.tap_count());
      gesture.data.tap.tapCount = details.tap_count();
      gesture.data.tap.width = details.bounding_box_f().width();
      gesture.data.tap.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_LONG_PRESS:
      gesture.setType(WebInputEvent::GestureLongPress);
      gesture.data.longPress.width = details.bounding_box_f().width();
      gesture.data.longPress.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_LONG_TAP:
      gesture.setType(WebInputEvent::GestureLongTap);
      gesture.data.longPress.width = details.bounding_box_f().width();
      gesture.data.longPress.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_TWO_FINGER_TAP:
      gesture.setType(blink::WebInputEvent::GestureTwoFingerTap);
      gesture.data.twoFingerTap.firstFingerWidth = details.first_finger_width();
      gesture.data.twoFingerTap.firstFingerHeight =
          details.first_finger_height();
      break;
    case ET_GESTURE_SCROLL_BEGIN:
      gesture.setType(WebInputEvent::GestureScrollBegin);
      gesture.data.scrollBegin.pointerCount = details.touch_points();
      gesture.data.scrollBegin.deltaXHint = details.scroll_x_hint();
      gesture.data.scrollBegin.deltaYHint = details.scroll_y_hint();
      gesture.data.scrollBegin.deltaHintUnits =
          static_cast<blink::WebGestureEvent::ScrollUnits>(
              details.scroll_begin_units());
      break;
    case ET_GESTURE_SCROLL_UPDATE:
      gesture.setType(WebInputEvent::GestureScrollUpdate);
      gesture.data.scrollUpdate.deltaX = details.scroll_x();
      gesture.data.scrollUpdate.deltaY = details.scroll_y();
      gesture.data.scrollUpdate.deltaUnits =
          static_cast<blink::WebGestureEvent::ScrollUnits>(
              details.scroll_update_units());
      gesture.data.scrollUpdate.previousUpdateInSequencePrevented =
          details.previous_scroll_update_in_sequence_prevented();
      break;
    case ET_GESTURE_SCROLL_END:
      gesture.setType(WebInputEvent::GestureScrollEnd);
      break;
    case ET_SCROLL_FLING_START:
      gesture.setType(WebInputEvent::GestureFlingStart);
      gesture.data.flingStart.velocityX = details.velocity_x();
      gesture.data.flingStart.velocityY = details.velocity_y();
      break;
    case ET_SCROLL_FLING_CANCEL:
      gesture.setType(WebInputEvent::GestureFlingCancel);
      break;
    case ET_GESTURE_PINCH_BEGIN:
      gesture.setType(WebInputEvent::GesturePinchBegin);
      break;
    case ET_GESTURE_PINCH_UPDATE:
      gesture.setType(WebInputEvent::GesturePinchUpdate);
      gesture.data.pinchUpdate.scale = details.scale();
      break;
    case ET_GESTURE_PINCH_END:
      gesture.setType(WebInputEvent::GesturePinchEnd);
      break;
    case ET_GESTURE_TAP_CANCEL:
      gesture.setType(WebInputEvent::GestureTapCancel);
      break;
    case ET_GESTURE_TAP_DOWN:
      gesture.setType(WebInputEvent::GestureTapDown);
      gesture.data.tapDown.width = details.bounding_box_f().width();
      gesture.data.tapDown.height = details.bounding_box_f().height();
      break;
    case ET_GESTURE_BEGIN:
    case ET_GESTURE_END:
    case ET_GESTURE_SWIPE:
      // The caller is responsible for discarding these gestures appropriately.
      gesture.setType(WebInputEvent::Undefined);
      break;
    default:
      NOTREACHED() << "EventType provided wasn't a valid gesture event: "
                   << details.type();
  }

  return gesture;
}

WebGestureEvent CreateWebGestureEventFromGestureEventData(
    const GestureEventData& data) {
  return CreateWebGestureEvent(data.details, data.time,
                               gfx::PointF(data.x, data.y),
                               gfx::PointF(data.raw_x, data.raw_y), data.flags,
                               data.unique_touch_event_id);
}

std::unique_ptr<blink::WebInputEvent> ScaleWebInputEvent(
    const blink::WebInputEvent& event,
    float scale) {
  return TranslateAndScaleWebInputEvent(event, gfx::Vector2d(0, 0), scale);
}

std::unique_ptr<blink::WebInputEvent> TranslateAndScaleWebInputEvent(
    const blink::WebInputEvent& event,
    const gfx::Vector2d& delta,
    float scale) {
  std::unique_ptr<blink::WebInputEvent> scaled_event;
  if (scale == 1.f && delta.IsZero())
    return scaled_event;
  if (event.type() == blink::WebMouseEvent::MouseWheel) {
    blink::WebMouseWheelEvent* wheel_event = new blink::WebMouseWheelEvent;
    scaled_event.reset(wheel_event);
    *wheel_event = static_cast<const blink::WebMouseWheelEvent&>(event);
    wheel_event->x += delta.x();
    wheel_event->y += delta.y();
    wheel_event->x *= scale;
    wheel_event->y *= scale;
    if (!wheel_event->scrollByPage) {
      wheel_event->deltaX *= scale;
      wheel_event->deltaY *= scale;
      wheel_event->wheelTicksX *= scale;
      wheel_event->wheelTicksY *= scale;
    }
  } else if (blink::WebInputEvent::isMouseEventType(event.type())) {
    blink::WebMouseEvent* mouse_event = new blink::WebMouseEvent;
    scaled_event.reset(mouse_event);
    *mouse_event = static_cast<const blink::WebMouseEvent&>(event);
    mouse_event->x += delta.x();
    mouse_event->y += delta.y();
    mouse_event->x *= scale;
    mouse_event->y *= scale;
    mouse_event->windowX = mouse_event->x;
    mouse_event->windowY = mouse_event->y;
    mouse_event->movementX *= scale;
    mouse_event->movementY *= scale;
  } else if (blink::WebInputEvent::isTouchEventType(event.type())) {
    blink::WebTouchEvent* touch_event = new blink::WebTouchEvent;
    scaled_event.reset(touch_event);
    *touch_event = static_cast<const blink::WebTouchEvent&>(event);
    for (unsigned i = 0; i < touch_event->touchesLength; i++) {
      touch_event->touches[i].position.x += delta.x();
      touch_event->touches[i].position.y += delta.y();
      touch_event->touches[i].position.x *= scale;
      touch_event->touches[i].position.y *= scale;
      touch_event->touches[i].radiusX *= scale;
      touch_event->touches[i].radiusY *= scale;
    }
  } else if (blink::WebInputEvent::isGestureEventType(event.type())) {
    blink::WebGestureEvent* gesture_event = new blink::WebGestureEvent;
    scaled_event.reset(gesture_event);
    *gesture_event = static_cast<const blink::WebGestureEvent&>(event);
    gesture_event->x += delta.x();
    gesture_event->y += delta.y();
    gesture_event->x *= scale;
    gesture_event->y *= scale;
    switch (gesture_event->type()) {
      case blink::WebInputEvent::GestureScrollUpdate:
        if (gesture_event->data.scrollUpdate.deltaUnits !=
            blink::WebGestureEvent::ScrollUnits::Page) {
          gesture_event->data.scrollUpdate.deltaX *= scale;
          gesture_event->data.scrollUpdate.deltaY *= scale;
        }
        break;
      case blink::WebInputEvent::GestureScrollBegin:
        if (gesture_event->data.scrollBegin.deltaHintUnits !=
            blink::WebGestureEvent::ScrollUnits::Page) {
          gesture_event->data.scrollBegin.deltaXHint *= scale;
          gesture_event->data.scrollBegin.deltaYHint *= scale;
        }
        break;

      case blink::WebInputEvent::GesturePinchUpdate:
        // Scale in pinch gesture is DSF agnostic.
        break;

      case blink::WebInputEvent::GestureDoubleTap:
      case blink::WebInputEvent::GestureTap:
      case blink::WebInputEvent::GestureTapUnconfirmed:
        gesture_event->data.tap.width *= scale;
        gesture_event->data.tap.height *= scale;
        break;

      case blink::WebInputEvent::GestureTapDown:
        gesture_event->data.tapDown.width *= scale;
        gesture_event->data.tapDown.height *= scale;
        break;

      case blink::WebInputEvent::GestureShowPress:
        gesture_event->data.showPress.width *= scale;
        gesture_event->data.showPress.height *= scale;
        break;

      case blink::WebInputEvent::GestureLongPress:
      case blink::WebInputEvent::GestureLongTap:
        gesture_event->data.longPress.width *= scale;
        gesture_event->data.longPress.height *= scale;
        break;

      case blink::WebInputEvent::GestureTwoFingerTap:
        gesture_event->data.twoFingerTap.firstFingerWidth *= scale;
        gesture_event->data.twoFingerTap.firstFingerHeight *= scale;
        break;

      case blink::WebInputEvent::GestureFlingStart:
        gesture_event->data.flingStart.velocityX *= scale;
        gesture_event->data.flingStart.velocityY *= scale;
        break;

      // These event does not have location data.
      case blink::WebInputEvent::GesturePinchBegin:
      case blink::WebInputEvent::GesturePinchEnd:
      case blink::WebInputEvent::GestureTapCancel:
      case blink::WebInputEvent::GestureFlingCancel:
      case blink::WebInputEvent::GestureScrollEnd:
        break;

      // TODO(oshima): Find out if ContextMenu needs to be scaled.
      default:
        break;
    }
  }
  return scaled_event;
}

WebInputEvent::Type ToWebMouseEventType(MotionEvent::Action action) {
  switch (action) {
    case MotionEvent::ACTION_DOWN:
    case MotionEvent::ACTION_BUTTON_PRESS:
      return WebInputEvent::MouseDown;
    case MotionEvent::ACTION_MOVE:
    case MotionEvent::ACTION_HOVER_MOVE:
      return WebInputEvent::MouseMove;
    case MotionEvent::ACTION_HOVER_ENTER:
      return WebInputEvent::MouseEnter;
    case MotionEvent::ACTION_HOVER_EXIT:
      return WebInputEvent::MouseLeave;
    case MotionEvent::ACTION_UP:
    case MotionEvent::ACTION_BUTTON_RELEASE:
      return WebInputEvent::MouseUp;
    case MotionEvent::ACTION_NONE:
    case MotionEvent::ACTION_CANCEL:
    case MotionEvent::ACTION_POINTER_DOWN:
    case MotionEvent::ACTION_POINTER_UP:
      break;
  }
  NOTREACHED() << "Invalid MotionEvent::Action = " << action;
  return WebInputEvent::Undefined;
}

void SetWebPointerPropertiesFromMotionEventData(
    WebPointerProperties& webPointerProperties,
    int pointer_id,
    float pressure,
    float orientation_rad,
    float tilt_rad,
    int android_buttons_changed,
    int tool_type) {

  webPointerProperties.id = pointer_id;
  webPointerProperties.force = pressure;

  if (tool_type == MotionEvent::TOOL_TYPE_STYLUS) {
    // A stylus points to a direction specified by orientation and tilts to
    // the opposite direction. Coordinate system is left-handed.
    float r = sin(tilt_rad);
    float z = cos(tilt_rad);
    webPointerProperties.tiltX =
        lround(atan2(sin(-orientation_rad) * r, z) * 180.f / M_PI);
    webPointerProperties.tiltY =
        lround(atan2(cos(-orientation_rad) * r, z) * 180.f / M_PI);
  } else {
    webPointerProperties.tiltX = webPointerProperties.tiltY = 0;
  }

  webPointerProperties.button = ToWebPointerButton(android_buttons_changed);
  webPointerProperties.pointerType = ToWebPointerType(tool_type);
}

int WebEventModifiersToEventFlags(int modifiers) {
  int flags = 0;

  if (modifiers & blink::WebInputEvent::ShiftKey)
    flags |= EF_SHIFT_DOWN;
  if (modifiers & blink::WebInputEvent::ControlKey)
    flags |= EF_CONTROL_DOWN;
  if (modifiers & blink::WebInputEvent::AltKey)
    flags |= EF_ALT_DOWN;
  if (modifiers & blink::WebInputEvent::MetaKey)
    flags |= EF_COMMAND_DOWN;
  if (modifiers & blink::WebInputEvent::CapsLockOn)
    flags |= EF_CAPS_LOCK_ON;
  if (modifiers & blink::WebInputEvent::NumLockOn)
    flags |= EF_NUM_LOCK_ON;
  if (modifiers & blink::WebInputEvent::ScrollLockOn)
    flags |= EF_SCROLL_LOCK_ON;
  if (modifiers & blink::WebInputEvent::LeftButtonDown)
    flags |= EF_LEFT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::MiddleButtonDown)
    flags |= EF_MIDDLE_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::RightButtonDown)
    flags |= EF_RIGHT_MOUSE_BUTTON;
  if (modifiers & blink::WebInputEvent::IsAutoRepeat)
    flags |= EF_IS_REPEAT;

  return flags;
}

blink::WebInputEvent::Modifiers DomCodeToWebInputEventModifiers(DomCode code) {
  switch (KeycodeConverter::DomCodeToLocation(code)) {
    case DomKeyLocation::LEFT:
      return blink::WebInputEvent::IsLeft;
    case DomKeyLocation::RIGHT:
      return blink::WebInputEvent::IsRight;
    case DomKeyLocation::NUMPAD:
      return blink::WebInputEvent::IsKeyPad;
    case DomKeyLocation::STANDARD:
      break;
  }
  return static_cast<blink::WebInputEvent::Modifiers>(0);
}

bool IsGestureScollOrPinch(WebInputEvent::Type type) {
  switch (type) {
    case blink::WebGestureEvent::GestureScrollBegin:
    case blink::WebGestureEvent::GestureScrollUpdate:
    case blink::WebGestureEvent::GestureScrollEnd:
    case blink::WebGestureEvent::GesturePinchBegin:
    case blink::WebGestureEvent::GesturePinchUpdate:
    case blink::WebGestureEvent::GesturePinchEnd:
      return true;
    default:
      return false;
  }
}

bool IsContinuousGestureEvent(WebInputEvent::Type type) {
  switch (type) {
    case blink::WebGestureEvent::GestureScrollUpdate:
    case blink::WebGestureEvent::GesturePinchUpdate:
      return true;
    default:
      return false;
  }
}

}  // namespace ui
