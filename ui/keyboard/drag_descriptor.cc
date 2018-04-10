// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/drag_descriptor.h"

#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace keyboard {

DragDescriptor::DragDescriptor(const gfx::Point& keyboard_location,
                               const gfx::Vector2d& click_offset,
                               bool is_touch_drag,
                               ui::PointerId pointer_id)
    : original_keyboard_location_(keyboard_location),
      original_click_offset_(click_offset),
      is_touch_drag_(is_touch_drag),
      pointer_id_(pointer_id) {}

}  // namespace keyboard
