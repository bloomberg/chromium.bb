// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_EVENT_H_
#define UI_AURA_EVENT_H_
#pragma once

#include "base/basictypes.h"
#include "base/event_types.h"
#include "base/time.h"
#include "ui/aura/aura_export.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"

namespace ui {
class Transform;
}

namespace aura {

class Window;

class AURA_EXPORT Event {
 public:
  const base::NativeEvent& native_event() const { return native_event_; }
  ui::EventType type() const { return type_; }
  const base::Time& time_stamp() const { return time_stamp_; }
  int flags() const { return flags_; }

  // The following methods return true if the respective keys were pressed at
  // the time the event was created.
  bool IsShiftDown() const { return (flags_ & ui::EF_SHIFT_DOWN) != 0; }
  bool IsControlDown() const { return (flags_ & ui::EF_CONTROL_DOWN) != 0; }
  bool IsCapsLockDown() const { return (flags_ & ui::EF_CAPS_LOCK_DOWN) != 0; }
  bool IsAltDown() const { return (flags_ & ui::EF_ALT_DOWN) != 0; }

 protected:
  Event(ui::EventType type, int flags);
  Event(const base::NativeEvent& native_event, ui::EventType type, int flags);
  Event(const Event& copy);
  void set_type(ui::EventType type) { type_ = type; }
  void set_flags(int flags) { flags_ = flags; }

 private:
  void operator=(const Event&);

  // Safely initializes the native event members of this class.
  void Init();
  void InitWithNativeEvent(const base::NativeEvent& native_event);

  base::NativeEvent native_event_;
  ui::EventType type_;
  base::Time time_stamp_;
  int flags_;
};

class AURA_EXPORT LocatedEvent : public Event {
 public:
  int x() const { return location_.x(); }
  int y() const { return location_.y(); }
  gfx::Point location() const { return location_; }

  void UpdateForTransform(const ui::Transform& transform);

 protected:
  explicit LocatedEvent(const base::NativeEvent& native_event);

  // Create a new LocatedEvent which is identical to the provided model.
  // If source / target windows are provided, the model location will be
  // converted from |source| coordinate system to |target| coordinate system.
  LocatedEvent(const LocatedEvent& model, Window* source, Window* target);

  // Used for synthetic events in testing.
  LocatedEvent(ui::EventType type, const gfx::Point& location, int flags);

  gfx::Point location_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocatedEvent);
};

class AURA_EXPORT MouseEvent : public LocatedEvent {
 public:
  explicit MouseEvent(const base::NativeEvent& native_event);

  // Create a new MouseEvent based on the provided model.
  // Uses the provided |type| and |flags| for the new event.
  // If source / target windows are provided, the model location will be
  // converted from |source| coordinate system to |target| coordinate system.
  MouseEvent(const MouseEvent& model, Window* source, Window* target);
  MouseEvent(const MouseEvent& model,
             Window* source,
             Window* target,
             ui::EventType type,
             int flags);

  // Used for synthetic events in testing.
  MouseEvent(ui::EventType type, const gfx::Point& location, int flags);

 private:
  DISALLOW_COPY_AND_ASSIGN(MouseEvent);
};

class AURA_EXPORT TouchEvent : public LocatedEvent {
 public:
  explicit TouchEvent(const base::NativeEvent& native_event);

  // Create a new TouchEvent which is identical to the provided model.
  // If source / target windows are provided, the model location will be
  // converted from |source| coordinate system to |target| coordinate system.
  TouchEvent(const TouchEvent& model, Window* source, Window* target);

  // Used for synthetic events in testing.
  TouchEvent(ui::EventType type, const gfx::Point& location, int touch_id);

  int touch_id() const { return touch_id_; }
  float radius_x() const { return radius_x_; }
  float radius_y() const { return radius_y_; }
  float rotation_angle() const { return rotation_angle_; }
  float force() const { return force_; }

 private:
  // The identity (typically finger) of the touch starting at 0 and incrementing
  // for each separable additional touch that the hardware can detect.
  const int touch_id_;

  // Radius of the X (major) axis of the touch ellipse. 1.0 if unknown.
  const float radius_x_;

  // Radius of the Y (minor) axis of the touch ellipse. 1.0 if unknown.
  const float radius_y_;

  // Angle of the major axis away from the X axis. Default 0.0.
  const float rotation_angle_;

  // Force (pressure) of the touch. Normalized to be [0, 1]. Default to be 0.0.
  const float force_;

  DISALLOW_COPY_AND_ASSIGN(TouchEvent);
};

class AURA_EXPORT KeyEvent : public Event {
 public:
  KeyEvent(const base::NativeEvent& native_event, bool is_char);

  // Used for synthetic events in testing.
  KeyEvent(ui::EventType type,
           ui::KeyboardCode key_code,
           int flags);

  // These setters allow an I18N virtual keyboard to fabricate a keyboard event
  // which does not have a corresponding ui::KeyboardCode (example: U+00E1 Latin
  // small letter A with acute, U+0410 Cyrillic capital letter A.)
  // GetCharacter() and GetUnmodifiedCharacter() return the character.
  void set_character(uint16 character) { character_ = character; }
  void set_unmodified_character(uint16 unmodified_character) {
    unmodified_character_ = unmodified_character;
  }

  // Gets the character generated by this key event. It only supports Unicode
  // BMP characters.
  uint16 GetCharacter() const;

  // Gets the character generated by this key event ignoring concurrently-held
  // modifiers (except shift).
  uint16 GetUnmodifiedCharacter() const;

  ui::KeyboardCode key_code() const { return key_code_; }
  bool is_char() const { return is_char_; }

 private:
  ui::KeyboardCode key_code_;
  // True if this is a translated character event (vs. a raw key down). Both
  // share the same type: ui::ET_KEY_PRESSED.
  bool is_char_;

  uint16 character_;
  uint16 unmodified_character_;
};

class AURA_EXPORT DropTargetEvent : public LocatedEvent {
 public:
  DropTargetEvent(const ui::OSExchangeData& data,
                  const gfx::Point& location,
                  int source_operations)
      : LocatedEvent(ui::ET_DROP_TARGET_EVENT, location, 0),
        data_(data),
        source_operations_(source_operations) {
  }

  const ui::OSExchangeData& data() const { return data_; }
  int source_operations() const { return source_operations_; }

 private:
  // Data associated with the drag/drop session.
  const ui::OSExchangeData& data_;

  // Bitmask of supported ui::DragDropTypes::DragOperation by the source.
  int source_operations_;

  DISALLOW_COPY_AND_ASSIGN(DropTargetEvent);
};

}  // namespace aura

#endif  // UI_AURA_EVENT_H_
