// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_utils.h"

#include <windowsx.h>

#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/point.h"

namespace views {

bool RepostLocatedEvent(gfx::NativeWindow window,
                        const ui::LocatedEvent& event) {
  if (!window)
    return false;

  // Determine whether the click was in the client area or not.
  // NOTE: WM_NCHITTEST coordinates are relative to the screen.
  const gfx::Point screen_loc = event.location();
  LRESULT nc_hit_result = SendMessage(window, WM_NCHITTEST, 0,
                                      MAKELPARAM(screen_loc.x(),
                                                 screen_loc.y()));
  const bool in_client_area = nc_hit_result == HTCLIENT;

  // TODO(sky): this isn't right. The event to generate should correspond with
  // the event we just got. MouseEvent only tells us what is down, which may
  // differ. Need to add ability to get changed button from MouseEvent.
  int event_type;
  int flags = event.flags();
  if (flags & ui::EF_LEFT_MOUSE_BUTTON) {
    event_type = in_client_area ? WM_LBUTTONDOWN : WM_NCLBUTTONDOWN;
  } else if (flags & ui::EF_MIDDLE_MOUSE_BUTTON) {
    event_type = in_client_area ? WM_MBUTTONDOWN : WM_NCMBUTTONDOWN;
  } else if (flags & ui::EF_RIGHT_MOUSE_BUTTON) {
    event_type = in_client_area ? WM_RBUTTONDOWN : WM_NCRBUTTONDOWN;
  } else {
    NOTREACHED();
    return false;
  }

  int window_x = screen_loc.x();
  int window_y = screen_loc.y();
  if (in_client_area) {
    RECT window_bounds;
    GetWindowRect(window, &window_bounds);
    window_x -= window_bounds.left;
    window_y -= window_bounds.top;
  }

  WPARAM target = in_client_area ? event.native_event().wParam : nc_hit_result;
  PostMessage(window, event_type, target, MAKELPARAM(window_x, window_y));
  return true;
}

}  // namespace views
