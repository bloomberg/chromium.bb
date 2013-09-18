// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_utils.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/events/event.h"
#include "ui/gfx/point.h"

using aura::client::ScreenPositionClient;

namespace views {

bool RepostLocatedEvent(gfx::NativeWindow window,
                        const ui::LocatedEvent& event) {
  if (!window)
    return false;

  aura::RootWindow* root_window = window->GetRootWindow();

  gfx::Point root_loc(event.location());
  ScreenPositionClient* spc = GetScreenPositionClient(root_window);
  if (!spc)
    return false;

  spc->ConvertPointFromScreen(root_window, &root_loc);

  scoped_ptr<ui::LocatedEvent> relocated;
  if (event.IsMouseEvent()) {
    const ui::MouseEvent& orig = static_cast<const ui::MouseEvent&>(event);
    relocated.reset(new ui::MouseEvent(orig));
  } else if (event.IsGestureEvent()) {
    const ui::GestureEvent& orig = static_cast<const ui::GestureEvent&>(event);
    relocated.reset(new ui::GestureEvent(orig));
  } else {
    NOTREACHED();
    return false;
  }
  relocated->set_location(root_loc);
  relocated->set_root_location(root_loc);

  root_window->RepostEvent(*relocated);
  return true;
}

}  // namespace views
