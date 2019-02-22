// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/mojo/event_struct_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/mojo/event_constants.mojom.h"
#include "ui/latency/mojo/latency_info_struct_traits.h"

namespace mojo {

namespace {

ui::mojom::LocationDataPtr CreateLocationData(const ui::LocatedEvent* event) {
  ui::mojom::LocationDataPtr location_data(ui::mojom::LocationData::New());
  location_data->relative_location = event->location_f();
  location_data->root_location = event->root_location_f();
  return location_data;
}

void UpdateEventLocation(const ui::mojom::PointerData& pointer_data,
                         EventUniquePtr* out) {
  // Set the float location, as the constructor only takes a gfx::Point.
  // This uses the event root_location field to store screen pixel
  // coordinates. See http://crbug.com/608547
  out->get()->AsLocatedEvent()->set_location_f(
      pointer_data.location->relative_location);
  out->get()->AsLocatedEvent()->set_root_location_f(
      pointer_data.location->root_location);
}

bool ReadPointerDetailsDeprecated(ui::mojom::EventType event_type,
                                  const ui::mojom::PointerData& pointer_data,
                                  ui::PointerDetails* out) {
  switch (pointer_data.kind) {
    case ui::mojom::PointerKind::MOUSE: {
      if (event_type == ui::mojom::EventType::POINTER_WHEEL_CHANGED ||
          event_type == ui::mojom::EventType::MOUSE_WHEEL_EVENT) {
        *out = ui::PointerDetails(
            ui::EventPointerType::POINTER_TYPE_MOUSE,
            gfx::Vector2d(static_cast<int>(pointer_data.wheel_data->delta_x),
                          static_cast<int>(pointer_data.wheel_data->delta_y)),
            ui::MouseEvent::kMousePointerId);
      } else {
        *out = ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE,
                                  ui::MouseEvent::kMousePointerId);
      }
      return true;
    }
    case ui::mojom::PointerKind::TOUCH:
    case ui::mojom::PointerKind::PEN: {
      ui::EventPointerType pointer_type;
      if (!EnumTraits<ui::mojom::PointerKind, ui::EventPointerType>::FromMojom(
              pointer_data.kind, &pointer_type))
        return false;
      const ui::mojom::BrushData& brush_data = *pointer_data.brush_data;
      *out = ui::PointerDetails(
          pointer_type, pointer_data.pointer_id, brush_data.width,
          brush_data.height, brush_data.pressure, brush_data.twist,
          brush_data.tilt_x, brush_data.tilt_y, brush_data.tangential_pressure);
      return true;
    }
    case ui::mojom::PointerKind::ERASER:
      // TODO(jamescook): Eraser support.
      NOTIMPLEMENTED();
      return false;
    case ui::mojom::PointerKind::UNKNOWN:
      return false;
  }
  NOTREACHED();
  return false;
}

bool ReadScrollData(ui::mojom::EventDataView* event,
                    base::TimeTicks time_stamp,
                    EventUniquePtr* out) {
  ui::mojom::ScrollDataPtr scroll_data;
  if (!event->ReadScrollData<ui::mojom::ScrollDataPtr>(&scroll_data))
    return false;

  *out = std::make_unique<ui::ScrollEvent>(
      mojo::ConvertTo<ui::EventType>(event->action()),
      gfx::Point(scroll_data->location->relative_location.x(),
                 scroll_data->location->relative_location.y()),
      time_stamp, event->flags(), scroll_data->x_offset, scroll_data->y_offset,
      scroll_data->x_offset_ordinal, scroll_data->y_offset_ordinal,
      scroll_data->finger_count, scroll_data->momentum_phase);
  return true;
}

bool ReadGestureData(ui::mojom::EventDataView* event,
                     base::TimeTicks time_stamp,
                     EventUniquePtr* out) {
  ui::mojom::GestureDataPtr gesture_data;
  if (!event->ReadGestureData<ui::mojom::GestureDataPtr>(&gesture_data))
    return false;

  *out = std::make_unique<ui::GestureEvent>(
      gesture_data->location->relative_location.x(),
      gesture_data->location->relative_location.y(), event->flags(), time_stamp,
      ui::GestureEventDetails(ConvertTo<ui::EventType>(event->action())));
  return true;
}

}  // namespace

static_assert(ui::mojom::kEventFlagNone == static_cast<int32_t>(ui::EF_NONE),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagIsSynthesized ==
                  static_cast<int32_t>(ui::EF_IS_SYNTHESIZED),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagShiftDown ==
                  static_cast<int32_t>(ui::EF_SHIFT_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagControlDown ==
                  static_cast<int32_t>(ui::EF_CONTROL_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagAltDown ==
                  static_cast<int32_t>(ui::EF_ALT_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagCommandDown ==
                  static_cast<int32_t>(ui::EF_COMMAND_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagAltgrDown ==
                  static_cast<int32_t>(ui::EF_ALTGR_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagMod3Down ==
                  static_cast<int32_t>(ui::EF_MOD3_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagNumLockOn ==
                  static_cast<int32_t>(ui::EF_NUM_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagCapsLockOn ==
                  static_cast<int32_t>(ui::EF_CAPS_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagScrollLockOn ==
                  static_cast<int32_t>(ui::EF_SCROLL_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagLeftMouseButton ==
                  static_cast<int32_t>(ui::EF_LEFT_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagMiddleMouseButton ==
                  static_cast<int32_t>(ui::EF_MIDDLE_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagRightMouseButton ==
                  static_cast<int32_t>(ui::EF_RIGHT_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagBackMouseButton ==
                  static_cast<int32_t>(ui::EF_BACK_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagForwardMouseButton ==
                  static_cast<int32_t>(ui::EF_FORWARD_MOUSE_BUTTON),
              "EVENT_FLAGS must match");

// static
ui::mojom::EventType TypeConverter<ui::mojom::EventType,
                                   ui::EventType>::Convert(ui::EventType type) {
  switch (type) {
    case ui::ET_UNKNOWN:
      return ui::mojom::EventType::UNKNOWN;
    case ui::ET_KEY_PRESSED:
      return ui::mojom::EventType::KEY_PRESSED;
    case ui::ET_KEY_RELEASED:
      return ui::mojom::EventType::KEY_RELEASED;
    case ui::ET_POINTER_DOWN:
      return ui::mojom::EventType::POINTER_DOWN;
    case ui::ET_POINTER_MOVED:
      return ui::mojom::EventType::POINTER_MOVED;
    case ui::ET_POINTER_UP:
      return ui::mojom::EventType::POINTER_UP;
    case ui::ET_POINTER_CANCELLED:
      return ui::mojom::EventType::POINTER_CANCELLED;
    case ui::ET_POINTER_ENTERED:
      return ui::mojom::EventType::POINTER_ENTERED;
    case ui::ET_POINTER_EXITED:
      return ui::mojom::EventType::POINTER_EXITED;
    case ui::ET_POINTER_WHEEL_CHANGED:
      return ui::mojom::EventType::POINTER_WHEEL_CHANGED;
    case ui::ET_POINTER_CAPTURE_CHANGED:
      return ui::mojom::EventType::POINTER_CAPTURE_CHANGED;
    case ui::ET_GESTURE_TAP:
      return ui::mojom::EventType::GESTURE_TAP;
    case ui::ET_SCROLL:
      return ui::mojom::EventType::SCROLL;
    case ui::ET_SCROLL_FLING_START:
      return ui::mojom::EventType::SCROLL_FLING_START;
    case ui::ET_SCROLL_FLING_CANCEL:
      return ui::mojom::EventType::SCROLL_FLING_CANCEL;
    case ui::ET_CANCEL_MODE:
      return ui::mojom::EventType::CANCEL_MODE;
    case ui::ET_MOUSE_PRESSED:
      return ui::mojom::EventType::MOUSE_PRESSED_EVENT;
    case ui::ET_MOUSE_DRAGGED:
      return ui::mojom::EventType::MOUSE_DRAGGED_EVENT;
    case ui::ET_MOUSE_RELEASED:
      return ui::mojom::EventType::MOUSE_RELEASED_EVENT;
    case ui::ET_MOUSE_MOVED:
      return ui::mojom::EventType::MOUSE_MOVED_EVENT;
    case ui::ET_MOUSE_ENTERED:
      return ui::mojom::EventType::MOUSE_ENTERED_EVENT;
    case ui::ET_MOUSE_EXITED:
      return ui::mojom::EventType::MOUSE_EXITED_EVENT;
    case ui::ET_MOUSEWHEEL:
      return ui::mojom::EventType::MOUSE_WHEEL_EVENT;
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      return ui::mojom::EventType::MOUSE_CAPTURE_CHANGED_EVENT;
    case ui::ET_TOUCH_RELEASED:
      return ui::mojom::EventType::TOUCH_RELEASED;
    case ui::ET_TOUCH_PRESSED:
      return ui::mojom::EventType::TOUCH_PRESSED;
    case ui::ET_TOUCH_MOVED:
      return ui::mojom::EventType::TOUCH_MOVED;
    case ui::ET_TOUCH_CANCELLED:
      return ui::mojom::EventType::TOUCH_CANCELLED;
    default:
      NOTREACHED() << "Using unknown event types closes connections:"
                   << ui::EventTypeName(type);
      break;
  }
  return ui::mojom::EventType::UNKNOWN;
}

// static
ui::EventType TypeConverter<ui::EventType, ui::mojom::EventType>::Convert(
    ui::mojom::EventType type) {
  switch (type) {
    case ui::mojom::EventType::UNKNOWN:
      return ui::ET_UNKNOWN;
    case ui::mojom::EventType::KEY_PRESSED:
      return ui::ET_KEY_PRESSED;
    case ui::mojom::EventType::KEY_RELEASED:
      return ui::ET_KEY_RELEASED;
    case ui::mojom::EventType::POINTER_DOWN:
      return ui::ET_POINTER_DOWN;
    case ui::mojom::EventType::POINTER_MOVED:
      return ui::ET_POINTER_MOVED;
    case ui::mojom::EventType::POINTER_UP:
      return ui::ET_POINTER_UP;
    case ui::mojom::EventType::POINTER_CANCELLED:
      return ui::ET_POINTER_CANCELLED;
    case ui::mojom::EventType::POINTER_ENTERED:
      return ui::ET_POINTER_ENTERED;
    case ui::mojom::EventType::POINTER_EXITED:
      return ui::ET_POINTER_EXITED;
    case ui::mojom::EventType::POINTER_WHEEL_CHANGED:
      return ui::ET_POINTER_WHEEL_CHANGED;
    case ui::mojom::EventType::POINTER_CAPTURE_CHANGED:
      return ui::ET_POINTER_CAPTURE_CHANGED;
    case ui::mojom::EventType::GESTURE_TAP:
      return ui::ET_GESTURE_TAP;
    case ui::mojom::EventType::SCROLL:
      return ui::ET_SCROLL;
    case ui::mojom::EventType::SCROLL_FLING_START:
      return ui::ET_SCROLL_FLING_START;
    case ui::mojom::EventType::SCROLL_FLING_CANCEL:
      return ui::ET_SCROLL_FLING_CANCEL;
    case ui::mojom::EventType::MOUSE_PRESSED_EVENT:
      return ui::ET_MOUSE_PRESSED;
    case ui::mojom::EventType::MOUSE_DRAGGED_EVENT:
      return ui::ET_MOUSE_DRAGGED;
    case ui::mojom::EventType::MOUSE_RELEASED_EVENT:
      return ui::ET_MOUSE_RELEASED;
    case ui::mojom::EventType::MOUSE_MOVED_EVENT:
      return ui::ET_MOUSE_MOVED;
    case ui::mojom::EventType::MOUSE_ENTERED_EVENT:
      return ui::ET_MOUSE_ENTERED;
    case ui::mojom::EventType::MOUSE_EXITED_EVENT:
      return ui::ET_MOUSE_EXITED;
    case ui::mojom::EventType::MOUSE_WHEEL_EVENT:
      return ui::ET_MOUSEWHEEL;
    case ui::mojom::EventType::MOUSE_CAPTURE_CHANGED_EVENT:
      return ui::ET_MOUSE_CAPTURE_CHANGED;
    case ui::mojom::EventType::TOUCH_RELEASED:
      return ui::ET_TOUCH_RELEASED;
    case ui::mojom::EventType::TOUCH_PRESSED:
      return ui::ET_TOUCH_PRESSED;
    case ui::mojom::EventType::TOUCH_MOVED:
      return ui::ET_TOUCH_MOVED;
    case ui::mojom::EventType::TOUCH_CANCELLED:
      return ui::ET_TOUCH_CANCELLED;
    default:
      NOTREACHED();
  }
  return ui::ET_UNKNOWN;
}

// static
ui::mojom::EventType
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::action(
    const EventUniquePtr& event) {
  return mojo::ConvertTo<ui::mojom::EventType>(event->type());
}

// static
int32_t StructTraits<ui::mojom::EventDataView, EventUniquePtr>::flags(
    const EventUniquePtr& event) {
  return event->flags();
}

// static
base::TimeTicks
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::time_stamp(
    const EventUniquePtr& event) {
  return event->time_stamp();
}

// static
const ui::LatencyInfo&
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::latency(
    const EventUniquePtr& event) {
  return *event->latency();
}

// static
ui::mojom::KeyDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::key_data(
    const EventUniquePtr& event) {
  if (!event->IsKeyEvent())
    return nullptr;

  const ui::KeyEvent* key_event = event->AsKeyEvent();
  ui::mojom::KeyDataPtr key_data(ui::mojom::KeyData::New());
  key_data->key_code = key_event->GetConflatedWindowsKeyCode();
  key_data->native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(key_event->code());
  key_data->is_char = key_event->is_char();
  key_data->character = key_event->GetCharacter();
  key_data->windows_key_code = static_cast<ui::mojom::KeyboardCode>(
      key_event->GetLocatedWindowsKeyboardCode());
  key_data->text = key_event->GetText();
  key_data->unmodified_text = key_event->GetUnmodifiedText();
  return key_data;
}

// static
ui::mojom::PointerDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::pointer_data(
    const EventUniquePtr& event) {
  if (!event->IsPointerEvent())
    return nullptr;

  ui::mojom::PointerDataPtr pointer_data(ui::mojom::PointerData::New());
  const ui::PointerEvent* pointer_event = event->AsPointerEvent();
  const ui::PointerDetails* pointer_details = &pointer_event->pointer_details();
  pointer_data->changed_button_flags = pointer_event->changed_button_flags();
  pointer_data->location = CreateLocationData(event->AsLocatedEvent());
  pointer_data->pointer_id = pointer_details->id;
  ui::EventPointerType pointer_type = pointer_details->pointer_type;

  switch (pointer_type) {
    case ui::EventPointerType::POINTER_TYPE_MOUSE:
      pointer_data->kind = ui::mojom::PointerKind::MOUSE;
      break;
    case ui::EventPointerType::POINTER_TYPE_TOUCH:
      pointer_data->kind = ui::mojom::PointerKind::TOUCH;
      break;
    case ui::EventPointerType::POINTER_TYPE_PEN:
      pointer_data->kind = ui::mojom::PointerKind::PEN;
      break;
    case ui::EventPointerType::POINTER_TYPE_ERASER:
      pointer_data->kind = ui::mojom::PointerKind::ERASER;
      break;
    case ui::EventPointerType::POINTER_TYPE_UNKNOWN:
      NOTREACHED();
  }

  ui::mojom::BrushDataPtr brush_data(ui::mojom::BrushData::New());
  // TODO(rjk): this is in the wrong coordinate system
  brush_data->width = pointer_details->radius_x;
  brush_data->height = pointer_details->radius_y;
  brush_data->pressure = pointer_details->force;
  // In theory only pen events should have tilt, tangential_pressure and twist.
  // In practive a JavaScript PointerEvent could have type touch and still have
  // data in those fields.
  brush_data->tilt_x = pointer_details->tilt_x;
  brush_data->tilt_y = pointer_details->tilt_y;
  brush_data->tangential_pressure = pointer_details->tangential_pressure;
  brush_data->twist = pointer_details->twist;
  pointer_data->brush_data = std::move(brush_data);

  // TODO(rjkroege): Plumb raw pointer events on windows.
  // TODO(rjkroege): Handle force-touch on MacOS
  // TODO(rjkroege): Adjust brush data appropriately for Android.

  if (event->type() == ui::ET_POINTER_WHEEL_CHANGED) {
    ui::mojom::WheelDataPtr wheel_data(ui::mojom::WheelData::New());

    // TODO(rjkroege): Support page scrolling on windows by directly
    // cracking into a mojo event when the native event is available.
    wheel_data->mode = ui::mojom::WheelMode::LINE;
    // TODO(rjkroege): Support precise scrolling deltas.

    if ((event->flags() & ui::EF_SHIFT_DOWN) != 0 &&
        pointer_details->offset.x() == 0) {
      wheel_data->delta_x = pointer_details->offset.y();
      wheel_data->delta_y = 0;
      wheel_data->delta_z = 0;
    } else {
      // TODO(rjkroege): support z in ui::Events.
      wheel_data->delta_x = pointer_details->offset.x();
      wheel_data->delta_y = pointer_details->offset.y();
      wheel_data->delta_z = 0;
    }
    pointer_data->wheel_data = std::move(wheel_data);
  }

  return pointer_data;
}

// static
ui::mojom::MouseDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::mouse_data(
    const EventUniquePtr& event) {
  if (!event->IsMouseEvent())
    return nullptr;

  const ui::MouseEvent* mouse_event = event->AsMouseEvent();
  ui::mojom::MouseDataPtr mouse_data(ui::mojom::MouseData::New());
  mouse_data->changed_button_flags = mouse_event->changed_button_flags();
  mouse_data->pointer_details = mouse_event->pointer_details();
  mouse_data->location = CreateLocationData(mouse_event);
  if (mouse_event->IsMouseWheelEvent())
    mouse_data->wheel_offset = mouse_event->AsMouseWheelEvent()->offset();
  return mouse_data;
}

// static
ui::mojom::GestureDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::gesture_data(
    const EventUniquePtr& event) {
  if (!event->IsGestureEvent())
    return nullptr;

  ui::mojom::GestureDataPtr gesture_data(ui::mojom::GestureData::New());
  gesture_data->location = CreateLocationData(event->AsLocatedEvent());
  return gesture_data;
}

// static
ui::mojom::ScrollDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::scroll_data(
    const EventUniquePtr& event) {
  if (!event->IsScrollEvent())
    return nullptr;

  ui::mojom::ScrollDataPtr scroll_data(ui::mojom::ScrollData::New());
  scroll_data->location = CreateLocationData(event->AsLocatedEvent());
  const ui::ScrollEvent* scroll_event = event->AsScrollEvent();
  scroll_data->x_offset = scroll_event->x_offset();
  scroll_data->y_offset = scroll_event->y_offset();
  scroll_data->x_offset_ordinal = scroll_event->x_offset_ordinal();
  scroll_data->y_offset_ordinal = scroll_event->y_offset_ordinal();
  scroll_data->finger_count = scroll_event->finger_count();
  scroll_data->momentum_phase = scroll_event->momentum_phase();
  return scroll_data;
}

// static
ui::mojom::TouchDataPtr
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::touch_data(
    const EventUniquePtr& event) {
  if (!event->IsTouchEvent())
    return nullptr;

  const ui::TouchEvent* touch_event = event->AsTouchEvent();
  ui::mojom::TouchDataPtr touch_data(ui::mojom::TouchData::New());
  touch_data->may_cause_scrolling = touch_event->may_cause_scrolling();
  touch_data->hovering = touch_event->hovering();
  touch_data->location = CreateLocationData(touch_event);
  touch_data->pointer_details = touch_event->pointer_details();
  return touch_data;
}

// static
base::flat_map<std::string, std::vector<uint8_t>>
StructTraits<ui::mojom::EventDataView, EventUniquePtr>::properties(
    const EventUniquePtr& event) {
  return event->properties() ? *(event->properties()) : ui::Event::Properties();
}

// static
bool StructTraits<ui::mojom::EventDataView, EventUniquePtr>::Read(
    ui::mojom::EventDataView event,
    EventUniquePtr* out) {
  DCHECK(!out->get());

  base::TimeTicks time_stamp;
  if (!event.ReadTimeStamp(&time_stamp))
    return false;

  switch (event.action()) {
    case ui::mojom::EventType::KEY_PRESSED:
    case ui::mojom::EventType::KEY_RELEASED: {
      ui::mojom::KeyDataPtr key_data;
      if (!event.ReadKeyData<ui::mojom::KeyDataPtr>(&key_data))
        return false;

      if (key_data->is_char) {
        *out = std::make_unique<ui::KeyEvent>(
            static_cast<base::char16>(key_data->character),
            static_cast<ui::KeyboardCode>(key_data->key_code),
            ui::DomCode::NONE, event.flags(), time_stamp);
      } else {
        *out = std::make_unique<ui::KeyEvent>(
            event.action() == ui::mojom::EventType::KEY_PRESSED
                ? ui::ET_KEY_PRESSED
                : ui::ET_KEY_RELEASED,
            static_cast<ui::KeyboardCode>(key_data->key_code), event.flags(),
            time_stamp);
      }
      break;
    }
    case ui::mojom::EventType::POINTER_DOWN:
    case ui::mojom::EventType::POINTER_UP:
    case ui::mojom::EventType::POINTER_MOVED:
    case ui::mojom::EventType::POINTER_CANCELLED:
    case ui::mojom::EventType::POINTER_ENTERED:
    case ui::mojom::EventType::POINTER_EXITED:
    case ui::mojom::EventType::POINTER_WHEEL_CHANGED:
    case ui::mojom::EventType::POINTER_CAPTURE_CHANGED: {
      ui::mojom::PointerDataPtr pointer_data;
      if (!event.ReadPointerData<ui::mojom::PointerDataPtr>(&pointer_data))
        return false;

      ui::PointerDetails pointer_details;
      if (!ReadPointerDetailsDeprecated(event.action(), *pointer_data,
                                        &pointer_details))
        return false;

      *out = std::make_unique<ui::PointerEvent>(
          mojo::ConvertTo<ui::EventType>(event.action()), gfx::Point(),
          gfx::Point(), event.flags(), pointer_data->changed_button_flags,
          pointer_details, time_stamp);

      UpdateEventLocation(*pointer_data, out);
      break;
    }
    case ui::mojom::EventType::GESTURE_TAP:
      if (!ReadGestureData(&event, time_stamp, out))
        return false;
      break;
    case ui::mojom::EventType::SCROLL:
      if (!ReadScrollData(&event, time_stamp, out))
        return false;
      break;
    case ui::mojom::EventType::SCROLL_FLING_START:
    case ui::mojom::EventType::SCROLL_FLING_CANCEL:
      // SCROLL_FLING_START/CANCEL is represented by a GestureEvent if
      // EF_FROM_TOUCH is set.
      if ((event.flags() & ui::EF_FROM_TOUCH) != 0) {
        if (!ReadGestureData(&event, time_stamp, out))
          return false;
      } else if (!ReadScrollData(&event, time_stamp, out)) {
        return false;
      }
      break;
    case ui::mojom::EventType::CANCEL_MODE:
      *out = std::make_unique<ui::CancelModeEvent>();
      break;
    case ui::mojom::EventType::MOUSE_PRESSED_EVENT:
    case ui::mojom::EventType::MOUSE_RELEASED_EVENT:
    case ui::mojom::EventType::MOUSE_DRAGGED_EVENT:
    case ui::mojom::EventType::MOUSE_MOVED_EVENT:
    case ui::mojom::EventType::MOUSE_ENTERED_EVENT:
    case ui::mojom::EventType::MOUSE_EXITED_EVENT:
    case ui::mojom::EventType::MOUSE_WHEEL_EVENT:
    case ui::mojom::EventType::MOUSE_CAPTURE_CHANGED_EVENT: {
      ui::mojom::MouseDataPtr mouse_data;
      if (!event.ReadMouseData(&mouse_data))
        return false;

      std::unique_ptr<ui::MouseEvent> mouse_event;
      if (event.action() == ui::mojom::EventType::MOUSE_WHEEL_EVENT) {
        mouse_event = std::make_unique<ui::MouseWheelEvent>(
            mouse_data->wheel_offset,
            gfx::Point(),  // Real location set below.
            gfx::Point(),  // Real location set below.
            time_stamp, event.flags(), mouse_data->changed_button_flags);
      } else {
        mouse_event = std::make_unique<ui::MouseEvent>(
            mojo::ConvertTo<ui::EventType>(event.action()),
            gfx::Point(),  // Real location set below.
            gfx::Point(),  // Real location set below.
            time_stamp, event.flags(), mouse_data->changed_button_flags,
            mouse_data->pointer_details);
      }
      mouse_event->set_location_f(mouse_data->location->relative_location);
      mouse_event->set_root_location_f(mouse_data->location->root_location);
      *out = std::move(mouse_event);
      break;
    }
    case ui::mojom::EventType::TOUCH_RELEASED:
    case ui::mojom::EventType::TOUCH_PRESSED:
    case ui::mojom::EventType::TOUCH_MOVED:
    case ui::mojom::EventType::TOUCH_CANCELLED: {
      ui::mojom::TouchDataPtr touch_data;
      if (!event.ReadTouchData(&touch_data))
        return false;
      std::unique_ptr<ui::TouchEvent> touch_event =
          std::make_unique<ui::TouchEvent>(
              mojo::ConvertTo<ui::EventType>(event.action()),
              gfx::Point(),  // Real location set below.
              time_stamp, touch_data->pointer_details, event.flags());
      touch_event->set_location_f(touch_data->location->relative_location);
      touch_event->set_root_location_f(touch_data->location->root_location);
      touch_event->set_may_cause_scrolling(touch_data->may_cause_scrolling);
      touch_event->set_hovering(touch_data->hovering);
      *out = std::move(touch_event);
      break;
    }
    case ui::mojom::EventType::UNKNOWN:
      NOTREACHED() << "Using unknown event types closes connections";
      return false;
  }

  if (!out->get())
    return false;

  if (!event.ReadLatency((*out)->latency()))
    return false;

  ui::Event::Properties properties;
  if (!event.ReadProperties(&properties))
    return false;
  if (!properties.empty())
    (*out)->SetProperties(properties);

  return true;
}

// static
bool StructTraits<ui::mojom::PointerDetailsDataView, ui::PointerDetails>::Read(
    ui::mojom::PointerDetailsDataView data,
    ui::PointerDetails* out) {
  if (!data.ReadPointerType(&out->pointer_type))
    return false;
  out->radius_x = data.radius_x();
  out->radius_y = data.radius_y();
  out->force = data.force();
  out->tilt_x = data.tilt_x();
  out->tilt_y = data.tilt_y();
  out->tangential_pressure = data.tangential_pressure();
  out->twist = data.twist();
  out->id = data.id();
  out->offset.set_x(data.offset_x());
  out->offset.set_y(data.offset_y());
  return true;
}

}  // namespace mojo
