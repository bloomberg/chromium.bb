// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/event_targeter.h"

#include "services/ui/ws/event_targeter_delegate.h"
#include "services/ui/ws/modal_window_controller.h"
#include "services/ui/ws/window_finder.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
namespace ws {

EventTargeter::EventTargeter(EventTargeterDelegate* event_targeter_delegate,
                             ModalWindowController* modal_window_controller)
    : event_targeter_delegate_(event_targeter_delegate),
      modal_window_controller_(modal_window_controller) {}

EventTargeter::~EventTargeter() {}

PointerTarget EventTargeter::PointerTargetForEvent(
    const ui::LocatedEvent& event,
    int64_t* display_id) {
  PointerTarget pointer_target;
  gfx::Point event_root_location(event.root_location());
  DeepestWindow deepest_window =
      FindDeepestVisibleWindowForEvents(&event_root_location, display_id);
  pointer_target.window =
      modal_window_controller_->GetTargetForWindow(deepest_window.window);
  pointer_target.is_mouse_event = event.IsMousePointerEvent();
  pointer_target.in_nonclient_area =
      deepest_window.window != pointer_target.window ||
      !pointer_target.window || deepest_window.in_non_client_area;
  pointer_target.is_pointer_down = event.type() == ui::ET_POINTER_DOWN;
  return pointer_target;
}

DeepestWindow EventTargeter::FindDeepestVisibleWindowForEvents(
    gfx::Point* location,
    int64_t* display_id) {
  ServerWindow* root =
      event_targeter_delegate_->GetRootWindowContaining(location, display_id);
  return root ? ui::ws::FindDeepestVisibleWindowForEvents(root, *location)
              : DeepestWindow();
}

}  // namespace ws
}  // namespace ui
