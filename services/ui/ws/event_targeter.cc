// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/event_targeter.h"

#include "services/ui/ws/event_targeter_delegate.h"

namespace ui {
namespace ws {

EventTargeter::EventTargeter(EventTargeterDelegate* event_targeter_delegate)
    : event_targeter_delegate_(event_targeter_delegate) {}

EventTargeter::~EventTargeter() {}

LocationTarget EventTargeter::FindTargetForLocation(const gfx::Point& location,
                                                    int64_t display_id) {
  LocationTarget location_target;
  location_target.location_in_root = location;
  location_target.display_id = display_id;
  ServerWindow* root = event_targeter_delegate_->GetRootWindowContaining(
      &location_target.location_in_root, &location_target.display_id);
  if (root) {
    location_target.deepest_window =
        ui::ws::FindDeepestVisibleWindowForLocation(
            root, location_target.location_in_root);
  }
  return location_target;
}

}  // namespace ws
}  // namespace ui
