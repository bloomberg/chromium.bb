// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_DRAG_DESCRIPTOR_H_
#define UI_KEYBOARD_DRAG_DESCRIPTOR_H_

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
                 const gfx::Vector2d& click_offset);

  gfx::Point original_keyboard_location() const {
    return original_keyboard_location_;
  }
  gfx::Vector2d original_click_offset() const { return original_click_offset_; }

 private:
  const gfx::Point original_keyboard_location_;
  const gfx::Vector2d original_click_offset_;
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_DRAG_DESCRIPTOR_H_
