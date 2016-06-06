// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/mojo/event_struct_traits.h"

#include "components/mus/public/interfaces/input_event_constants.mojom.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace mojo {
namespace {

mus::mojom::EventType UIEventTypeToMojo(ui::EventType type) {
  switch (type) {
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_POINTER_DOWN:
      return mus::mojom::EventType::POINTER_DOWN;

    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_TOUCH_MOVED:
    case ui::ET_POINTER_MOVED:
      return mus::mojom::EventType::POINTER_MOVE;

    case ui::ET_MOUSE_EXITED:
    case ui::ET_POINTER_EXITED:
      return mus::mojom::EventType::MOUSE_EXIT;

    case ui::ET_MOUSEWHEEL:
      return mus::mojom::EventType::WHEEL;

    case ui::ET_MOUSE_RELEASED:
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_POINTER_UP:
      return mus::mojom::EventType::POINTER_UP;

    case ui::ET_TOUCH_CANCELLED:
    case ui::ET_POINTER_CANCELLED:
      return mus::mojom::EventType::POINTER_CANCEL;

    case ui::ET_KEY_PRESSED:
      return mus::mojom::EventType::KEY_PRESSED;

    case ui::ET_KEY_RELEASED:
      return mus::mojom::EventType::KEY_RELEASED;

    default:
      break;
  }
  return mus::mojom::EventType::UNKNOWN;
}

ui::EventType MojoMouseEventTypeToUIEvent(mus::mojom::EventType action,
                                          int32_t flags) {
  switch (action) {
    case mus::mojom::EventType::POINTER_DOWN:
      return ui::ET_MOUSE_PRESSED;

    case mus::mojom::EventType::POINTER_UP:
      return ui::ET_MOUSE_RELEASED;

    case mus::mojom::EventType::POINTER_MOVE:
      if (flags & (mus::mojom::kEventFlagLeftMouseButton |
                   mus::mojom::kEventFlagMiddleMouseButton |
                   mus::mojom::kEventFlagRightMouseButton)) {
        return ui::ET_MOUSE_DRAGGED;
      }
      return ui::ET_MOUSE_MOVED;

    case mus::mojom::EventType::MOUSE_EXIT:
      return ui::ET_MOUSE_EXITED;

    default:
      NOTREACHED();
  }

  return ui::ET_MOUSE_RELEASED;
}

ui::EventType MojoTouchEventTypeToUIEvent(mus::mojom::EventType action) {
  switch (action) {
    case mus::mojom::EventType::POINTER_DOWN:
      return ui::ET_TOUCH_PRESSED;

    case mus::mojom::EventType::POINTER_UP:
      return ui::ET_TOUCH_RELEASED;

    case mus::mojom::EventType::POINTER_MOVE:
      return ui::ET_TOUCH_MOVED;

    case mus::mojom::EventType::POINTER_CANCEL:
      return ui::ET_TOUCH_CANCELLED;

    default:
      NOTREACHED();
  }

  return ui::ET_TOUCH_CANCELLED;
}

}  // namespace

static_assert(mus::mojom::kEventFlagNone == static_cast<int32_t>(ui::EF_NONE),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagIsSynthesized ==
                  static_cast<int32_t>(ui::EF_IS_SYNTHESIZED),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagShiftDown ==
                  static_cast<int32_t>(ui::EF_SHIFT_DOWN),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagControlDown ==
                  static_cast<int32_t>(ui::EF_CONTROL_DOWN),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagAltDown ==
                  static_cast<int32_t>(ui::EF_ALT_DOWN),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagCommandDown ==
                  static_cast<int32_t>(ui::EF_COMMAND_DOWN),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagAltgrDown ==
                  static_cast<int32_t>(ui::EF_ALTGR_DOWN),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagMod3Down ==
                  static_cast<int32_t>(ui::EF_MOD3_DOWN),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagNumLockOn ==
                  static_cast<int32_t>(ui::EF_NUM_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagCapsLockOn ==
                  static_cast<int32_t>(ui::EF_CAPS_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagScrollLockOn ==
                  static_cast<int32_t>(ui::EF_SCROLL_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagLeftMouseButton ==
                  static_cast<int32_t>(ui::EF_LEFT_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagMiddleMouseButton ==
                  static_cast<int32_t>(ui::EF_MIDDLE_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagRightMouseButton ==
                  static_cast<int32_t>(ui::EF_RIGHT_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagBackMouseButton ==
                  static_cast<int32_t>(ui::EF_BACK_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(mus::mojom::kEventFlagForwardMouseButton ==
                  static_cast<int32_t>(ui::EF_FORWARD_MOUSE_BUTTON),
              "EVENT_FLAGS must match");

int32_t StructTraits<mus::mojom::Event, EventUniquePtr>::action(
    const EventUniquePtr& event) {
  return static_cast<int32_t>(UIEventTypeToMojo(event->type()));
}

int32_t StructTraits<mus::mojom::Event, EventUniquePtr>::flags(
    const EventUniquePtr& event) {
  return event->flags();
}

int64_t StructTraits<mus::mojom::Event, EventUniquePtr>::time_stamp(
    const EventUniquePtr& event) {
  return event->time_stamp().ToInternalValue();
}

bool StructTraits<mus::mojom::Event, EventUniquePtr>::has_key_data(
    const EventUniquePtr& event) {
  return event->IsKeyEvent();
}

mus::mojom::KeyDataPtr
StructTraits<mus::mojom::Event, EventUniquePtr>::key_data(
    const EventUniquePtr& event) {
  if (!event->IsKeyEvent())
    return nullptr;

  const ui::KeyEvent* key_event = event->AsKeyEvent();
  mus::mojom::KeyDataPtr key_data(mus::mojom::KeyData::New());
  key_data->key_code = key_event->GetConflatedWindowsKeyCode();
  key_data->native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(key_event->code());
  key_data->is_char = key_event->is_char();
  key_data->character = key_event->GetCharacter();
  key_data->windows_key_code = static_cast<mus::mojom::KeyboardCode>(
      key_event->GetLocatedWindowsKeyboardCode());
  key_data->text = key_event->GetText();
  key_data->unmodified_text = key_event->GetUnmodifiedText();

  return key_data;
}

bool StructTraits<mus::mojom::Event, EventUniquePtr>::has_pointer_data(
    const EventUniquePtr& event) {
  return event->IsPointerEvent() || event->IsMouseWheelEvent();
}

mus::mojom::PointerDataPtr
StructTraits<mus::mojom::Event, EventUniquePtr>::pointer_data(
    const EventUniquePtr& event) {
  if (!has_pointer_data(event))
    return nullptr;

  mus::mojom::PointerDataPtr pointer_data(mus::mojom::PointerData::New());

  const ui::PointerDetails* pointer_details = nullptr;
  if (event->IsPointerEvent()) {
    const ui::PointerEvent* pointer_event = event->AsPointerEvent();
    pointer_data->pointer_id = pointer_event->pointer_id();
    pointer_details = &pointer_event->pointer_details();
  } else {
    const ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();
    pointer_data->pointer_id = ui::PointerEvent::kMousePointerId;
    pointer_details = &wheel_event->pointer_details();
  }

  switch (pointer_details->pointer_type) {
    case ui::EventPointerType::POINTER_TYPE_MOUSE:
      pointer_data->kind = mus::mojom::PointerKind::MOUSE;
      break;
    case ui::EventPointerType::POINTER_TYPE_TOUCH:
      pointer_data->kind = mus::mojom::PointerKind::TOUCH;
      break;
    default:
      NOTREACHED();
  }

  mus::mojom::BrushDataPtr brush_data(mus::mojom::BrushData::New());
  // TODO(rjk): this is in the wrong coordinate system
  brush_data->width = pointer_details->radius_x;
  brush_data->height = pointer_details->radius_y;
  // TODO(rjk): update for touch_event->rotation_angle();
  brush_data->pressure = pointer_details->force;
  brush_data->tilt_x = pointer_details->tilt_x;
  brush_data->tilt_y = pointer_details->tilt_y;
  pointer_data->brush_data = std::move(brush_data);

  // TODO(rjkroege): Plumb raw pointer events on windows.
  // TODO(rjkroege): Handle force-touch on MacOS
  // TODO(rjkroege): Adjust brush data appropriately for Android.

  mus::mojom::LocationDataPtr location_data(mus::mojom::LocationData::New());
  const ui::LocatedEvent* located_event = event->AsLocatedEvent();
  location_data->x = located_event->location_f().x();
  location_data->y = located_event->location_f().y();
  location_data->screen_x = located_event->root_location_f().x();
  location_data->screen_y = located_event->root_location_f().y();
  pointer_data->location = std::move(location_data);

  if (event->IsMouseWheelEvent()) {
    const ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();

    mus::mojom::WheelDataPtr wheel_data(mus::mojom::WheelData::New());

    // TODO(rjkroege): Support page scrolling on windows by directly
    // cracking into a mojo event when the native event is available.
    wheel_data->mode = mus::mojom::WheelMode::LINE;
    // TODO(rjkroege): Support precise scrolling deltas.

    if ((event->flags() & ui::EF_SHIFT_DOWN) != 0 &&
        wheel_event->x_offset() == 0) {
      wheel_data->delta_x = wheel_event->y_offset();
      wheel_data->delta_y = 0;
      wheel_data->delta_z = 0;
    } else {
      // TODO(rjkroege): support z in ui::Events.
      wheel_data->delta_x = wheel_event->x_offset();
      wheel_data->delta_y = wheel_event->y_offset();
      wheel_data->delta_z = 0;
    }
    pointer_data->wheel_data = std::move(wheel_data);
  }

  return pointer_data;
}

bool StructTraits<mus::mojom::Event, EventUniquePtr>::Read(
    mus::mojom::EventDataView event,
    EventUniquePtr* out) {
  switch (event.action()) {
    case mus::mojom::EventType::KEY_PRESSED:
    case mus::mojom::EventType::KEY_RELEASED: {
      mus::mojom::KeyDataPtr key_data;
      if (!event.ReadKeyData<mus::mojom::KeyDataPtr>(&key_data))
        return false;

      if (key_data->is_char) {
        out->reset(new ui::KeyEvent(
            static_cast<base::char16>(key_data->character),
            static_cast<ui::KeyboardCode>(key_data->key_code), event.flags()));
        return true;
      }
      out->reset(new ui::KeyEvent(
          event.action() == mus::mojom::EventType::KEY_PRESSED
              ? ui::ET_KEY_PRESSED
              : ui::ET_KEY_RELEASED,

          static_cast<ui::KeyboardCode>(key_data->key_code), event.flags()));
      return true;
    }
    case mus::mojom::EventType::POINTER_DOWN:
    case mus::mojom::EventType::POINTER_UP:
    case mus::mojom::EventType::POINTER_MOVE:
    case mus::mojom::EventType::POINTER_CANCEL:
    case mus::mojom::EventType::MOUSE_EXIT:
    case mus::mojom::EventType::WHEEL: {
      mus::mojom::PointerDataPtr pointer_data;
      if (!event.ReadPointerData<mus::mojom::PointerDataPtr>(&pointer_data))
        return false;

      const gfx::Point location(pointer_data->location->x,
                                pointer_data->location->y);
      const gfx::Point screen_location(pointer_data->location->screen_x,
                                       pointer_data->location->screen_y);

      switch (pointer_data->kind) {
        case mus::mojom::PointerKind::MOUSE: {
          if (event.action() == mus::mojom::EventType::WHEEL) {
            out->reset(new ui::MouseWheelEvent(
                gfx::Vector2d(
                    static_cast<int>(pointer_data->wheel_data->delta_x),
                    static_cast<int>(pointer_data->wheel_data->delta_y)),
                location, screen_location, ui::EventTimeForNow(),
                ui::EventFlags(event.flags()), ui::EventFlags(event.flags())));
            return true;
          }
          // TODO(moshayedi): Construct pointer event directly.
          ui::PointerEvent* mouse_event = new ui::PointerEvent(ui::MouseEvent(
              MojoMouseEventTypeToUIEvent(event.action(), event.flags()),
              location, screen_location, ui::EventTimeForNow(),
              ui::EventFlags(event.flags()), ui::EventFlags(event.flags())));
          out->reset(mouse_event);
          return true;
        }
        case mus::mojom::PointerKind::TOUCH: {
          // TODO(moshayedi): Construct pointer event directly.
          ui::PointerEvent* pointer_event = new ui::PointerEvent(ui::TouchEvent(
              MojoTouchEventTypeToUIEvent(event.action()), gfx::Point(),
              ui::EventFlags(event.flags()), pointer_data->pointer_id,
              base::TimeDelta::FromInternalValue(event.time_stamp()),
              pointer_data->brush_data->width, pointer_data->brush_data->height,
              0, pointer_data->brush_data->pressure));
          pointer_event->set_location(location);
          pointer_event->set_root_location(screen_location);
          out->reset(pointer_event);
          return true;
        }
        case mus::mojom::PointerKind::PEN:
          NOTIMPLEMENTED();
          return false;
      }
    }
    case mus::mojom::EventType::UNKNOWN:
      return false;
  }

  return false;
}

}  // namespace mojo