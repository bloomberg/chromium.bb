// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_utils.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/gfx/point.h"
#include "ui/views/views_delegate.h"

using aura::client::ScreenPositionClient;

namespace views {

bool RepostLocatedEvent(gfx::NativeWindow window,
                        const ui::LocatedEvent& event) {
#if defined(OS_WIN)
  // On Windows, if the |window| parameter is NULL, then we attempt to repost
  // the event to the window at the current location, if it is on the current
  // thread.
  HWND target_window = NULL;
  if (!window) {
    target_window = ::WindowFromPoint(event.location().ToPOINT());
    if (::GetWindowThreadProcessId(target_window, NULL) !=
        ::GetCurrentThreadId())
      return false;
  } else {
    if (ViewsDelegate::views_delegate &&
        !ViewsDelegate::views_delegate->IsWindowInMetro(window))
      target_window = window->GetHost()->GetAcceleratedWidget();
  }
  return RepostLocatedEventWin(target_window, event);
#endif
  if (!window)
    return false;

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

  root_window->GetHost()->dispatcher()->RepostEvent(*relocated);
  return true;
}

}  // namespace views
