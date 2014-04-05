// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_message_pump_dispatcher_win.h"

#include <windowsx.h>

#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/menu/menu_controller.h"

namespace views {
namespace internal {

MenuMessagePumpDispatcher::MenuMessagePumpDispatcher(MenuController* controller)
    : menu_controller_(controller) {}

MenuMessagePumpDispatcher::~MenuMessagePumpDispatcher() {}

uint32_t MenuMessagePumpDispatcher::Dispatch(const MSG& msg) {
  DCHECK(menu_controller_->IsBlockingRun());

  if (menu_controller_->exit_type() == MenuController::EXIT_ALL ||
      menu_controller_->exit_type() == MenuController::EXIT_DESTROYED)
    return (POST_DISPATCH_QUIT_LOOP | POST_DISPATCH_PERFORM_DEFAULT);

  // NOTE: we don't get WM_ACTIVATE or anything else interesting in here.
  switch (msg.message) {
    case WM_CONTEXTMENU: {
      MenuItemView* item = menu_controller_->pending_state_.item;
      if (item && item->GetRootMenuItem() != item) {
        gfx::Point screen_loc(0, item->height());
        View::ConvertPointToScreen(item, &screen_loc);
        ui::MenuSourceType source_type = ui::MENU_SOURCE_MOUSE;
        if (GET_X_LPARAM(msg.lParam) == -1 && GET_Y_LPARAM(msg.lParam) == -1)
          source_type = ui::MENU_SOURCE_KEYBOARD;
        item->GetDelegate()->ShowContextMenu(
            item, item->GetCommand(), screen_loc, source_type);
      }
      return POST_DISPATCH_NONE;
    }

    // NOTE: focus wasn't changed when the menu was shown. As such, don't
    // dispatch key events otherwise the focused window will get the events.
    case WM_KEYDOWN: {
      bool result =
          menu_controller_->OnKeyDown(ui::KeyboardCodeFromNative(msg));
      TranslateMessage(&msg);
      return result ? POST_DISPATCH_NONE : POST_DISPATCH_QUIT_LOOP;
    }
    case WM_CHAR: {
      bool should_exit =
          menu_controller_->SelectByChar(static_cast<base::char16>(msg.wParam));
      return should_exit ? POST_DISPATCH_QUIT_LOOP : POST_DISPATCH_NONE;
    }
    case WM_KEYUP:
      return POST_DISPATCH_NONE;

    case WM_SYSKEYUP:
      // We may have been shown on a system key, as such don't do anything
      // here. If another system key is pushed we'll get a WM_SYSKEYDOWN and
      // close the menu.
      return POST_DISPATCH_NONE;

    case WM_CANCELMODE:
    case WM_SYSKEYDOWN:
      // Exit immediately on system keys.
      menu_controller_->Cancel(MenuController::EXIT_ALL);
      return POST_DISPATCH_QUIT_LOOP;

    default:
      break;
  }
  return POST_DISPATCH_PERFORM_DEFAULT |
         (menu_controller_->exit_type() == MenuController::EXIT_NONE
              ? POST_DISPATCH_NONE
              : POST_DISPATCH_QUIT_LOOP);
}

}  // namespace internal
}  // namespace views
