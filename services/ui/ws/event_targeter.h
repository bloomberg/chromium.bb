// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_EVENT_TARGETER_H_
#define SERVICES_UI_WS_EVENT_TARGETER_H_

#include <stdint.h>

#include "base/macros.h"
#include "services/ui/ws/window_finder.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
namespace ws {
class EventTargeterDelegate;

// The target |window| for a given location, |location| and |display_id| are
// associated with the display |window| is on.
struct LocationTarget {
  DeepestWindow deepest_window;
  gfx::Point location_in_root;
  int64_t display_id = display::kInvalidDisplayId;
};

// Finds the target window for a location.
class EventTargeter {
 public:
  explicit EventTargeter(EventTargeterDelegate* event_targeter_delegate);
  ~EventTargeter();

  // Returns a LocationTarget for the supplied |location|. If there is no valid
  // root window, |window| in the returned value is null.
  LocationTarget FindTargetForLocation(const gfx::Point& location,
                                       int64_t display_id);

 private:
  EventTargeterDelegate* event_targeter_delegate_;

  DISALLOW_COPY_AND_ASSIGN(EventTargeter);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_EVENT_TARGETER_H_
