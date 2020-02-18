// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_FUCHSIA_INPUT_EVENT_DISPATCHER_H_
#define UI_EVENTS_FUCHSIA_INPUT_EVENT_DISPATCHER_H_

#include <fuchsia/ui/input/cpp/fidl.h>

#include "base/macros.h"
#include "ui/events/events_export.h"

namespace ui {

class InputEventDispatcherDelegate;

// Translates Fuchsia input events to Chrome ui::Events.
class EVENTS_EXPORT InputEventDispatcher {
 public:
  // |delegate|: The recipient of any Chrome events that are processed from
  // Fuchsia events.
  explicit InputEventDispatcher(InputEventDispatcherDelegate* delegate);
  ~InputEventDispatcher();

  // Processes a Fuchsia |event| and dispatches Chrome ui::Events from it.
  // |event|'s coordinates must be specified in device-independent pixels.
  //
  // Returns true if the event was processed.
  bool ProcessEvent(const fuchsia::ui::input::InputEvent& event) const;

 private:
  bool ProcessMouseEvent(const fuchsia::ui::input::PointerEvent& event) const;
  bool ProcessTouchEvent(const fuchsia::ui::input::PointerEvent& event) const;
  bool ProcessKeyboardEvent(
      const fuchsia::ui::input::KeyboardEvent& event) const;

  InputEventDispatcherDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(InputEventDispatcher);
};

}  // namespace ui

#endif  // UI_EVENTS_FUCHSIA_INPUT_EVENT_DISPATCHER_H_
