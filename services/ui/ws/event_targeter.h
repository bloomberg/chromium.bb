// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EVENT_TARGETER_H_
#define SERVICES_UI_WS_EVENT_TARGETER_H_

#include <stdint.h>

#include "base/macros.h"

namespace gfx {
class Point;
}

namespace ui {
class LocatedEvent;

namespace ws {
struct DeepestWindow;
class EventTargeterDelegate;
class ModalWindowController;
class ServerWindow;

// Keeps track of state associated with an active pointer.
struct PointerTarget {
  PointerTarget()
      : window(nullptr),
        is_mouse_event(false),
        in_nonclient_area(false),
        is_pointer_down(false) {}

  // The target window, which may be null. null is used in two situations:
  // when there is no valid window target, or there was a target but the
  // window is destroyed before a corresponding release/cancel.
  ServerWindow* window;

  bool is_mouse_event;

  // Did the pointer event start in the non-client area.
  bool in_nonclient_area;

  bool is_pointer_down;
};

// Finds the PointerTarget for an event or the DeepestWindow for a location.
class EventTargeter {
 public:
  EventTargeter(EventTargeterDelegate* event_targeter_delegate,
                ModalWindowController* modal_window_controller);
  ~EventTargeter();

  // Returns a PointerTarget for the supplied |event|. If there is no valid
  // event target for the specified location |window| in the returned value is
  // null.
  PointerTarget PointerTargetForEvent(const ui::LocatedEvent& event,
                                      int64_t* display_id);

  // Returns a DeepestWindow for the supplied |location|. If there is no valid
  // root window, |window| in the returned value is null.
  DeepestWindow FindDeepestVisibleWindowForEvents(gfx::Point* location,
                                                  int64_t* display_id);

 private:
  EventTargeterDelegate* event_targeter_delegate_;
  ModalWindowController* modal_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(EventTargeter);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EVENT_TARGETER_H_
