// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_DRAG_DESCRIPTOR_H_
#define UI_KEYBOARD_DRAG_DESCRIPTOR_H_

#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace keyboard {

// Tracks the state of a mouse drag to move the keyboard. The DragDescriptor
// does not actually change while the user drags. It essentially just records
// the offset of the original click on the keyboard along with the original
// location of the keyboard and uses incoming mouse move events to determine
// where the keyboard should be placed using those offsets.
class DragDescriptor {
 public:
  DragDescriptor(const gfx::Point& keyboard_location,
                 const gfx::Vector2d& click_offset,
                 bool is_touch_drag,
                 ui::PointerId pointer_id);

  gfx::Point original_keyboard_location() const {
    return original_keyboard_location_;
  }
  gfx::Vector2d original_click_offset() const { return original_click_offset_; }
  bool is_touch_drag() { return is_touch_drag_; }
  ui::PointerId pointer_id() { return pointer_id_; }

 private:
  const gfx::Point original_keyboard_location_;
  const gfx::Vector2d original_click_offset_;

  // Distinguish whether the current drag is from a touch event or mouse event,
  // so drag/move events can be filtered accordingly
  const bool is_touch_drag_;

  // The pointer ID provided by the touch event to disambiguate multiple
  // touch points. If this is a mouse event, then this value is -1.
  const ui::PointerId pointer_id_;
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_DRAG_DESCRIPTOR_H_
