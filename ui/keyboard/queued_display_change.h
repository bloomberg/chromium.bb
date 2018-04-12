// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_QUEUED_DISPLAY_CHANGE_H_
#define UI_KEYBOARD_QUEUED_DISPLAY_CHANGE_H_

#include "base/bind.h"
#include "base/optional.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/container_type.h"

namespace keyboard {

// TODO: refactor and merge this into QueuedContainerType and rename it to
// something like QueuedVisualChange or similar.
class QueuedDisplayChange {
 public:
  QueuedDisplayChange(const display::Display& display_info);
  ~QueuedDisplayChange();

  display::Display new_display() { return new_display_; }

 private:
  display::Display new_display_;
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_QUEUED_DISPLAY_CHANGE_H_
