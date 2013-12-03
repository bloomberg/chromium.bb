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
#include "ui/views/views_delegate.h"

using aura::client::ScreenPositionClient;

namespace views {

bool RepostLocatedEvent(gfx::NativeWindow window,
                        const ui::LocatedEvent& event) {
  if (!window)
    return false;

#if defined(OS_WIN)
  if (ViewsDelegate::views_delegate &&
      !ViewsDelegate::views_delegate->IsWindowInMetro(window)) {
    return RepostLocatedEventWin(
        window->GetDispatcher()->host()->GetAcceleratedWidget(), event);
  }
#endif
  aura::Window* root_window = window->GetRootWindow();

  gfx::Point root_loc(event.location());
  ScreenPositionClient* spc =
      aura::client::GetScreenPositionClient(root_window);
  if (!spc)
    return false;

  spc->ConvertPointFromScreen(root_window, &root_loc);

  scoped_ptr<ui::LocatedEvent> relocated;
  if (event.IsMouseEvent()) {
    const ui::MouseEvent& orig = static_cast<const ui::MouseEvent&>(event);
    relocated.reset(new ui::MouseEvent(orig));
  } else if (event.IsGestureEvent()) {
    // TODO(rbyers): Gesture event repost is tricky to get right
    // crbug.com/170987.
    return false;
  } else {
    NOTREACHED();
    return false;
  }
  relocated->set_location(root_loc);
  relocated->set_root_location(root_loc);

  root_window->GetDispatcher()->RepostEvent(*relocated);
  return true;
}

}  // namespace views
