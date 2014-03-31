// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_message_pump_dispatcher.h"

#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/menu/menu_controller.h"

namespace views {
namespace internal {

uint32_t MenuMessagePumpDispatcher::Dispatch(const base::NativeEvent& event) {
  if (menu_controller_->exit_type() == MenuController::EXIT_ALL ||
      menu_controller_->exit_type() == MenuController::EXIT_DESTROYED)
    return (POST_DISPATCH_QUIT_LOOP | POST_DISPATCH_PERFORM_DEFAULT);

  switch (ui::EventTypeFromNative(event)) {
    case ui::ET_KEY_PRESSED: {
      if (!menu_controller_->OnKeyDown(ui::KeyboardCodeFromNative(event)))
        return POST_DISPATCH_QUIT_LOOP;

      // Do not check mnemonics if the Alt or Ctrl modifiers are pressed.
      int flags = ui::EventFlagsFromNative(event);
      if ((flags & (ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)) == 0) {
        char c = ui::GetCharacterFromKeyCode(ui::KeyboardCodeFromNative(event),
                                             flags);
        if (menu_controller_->SelectByChar(c))
          return POST_DISPATCH_QUIT_LOOP;
      }
      return POST_DISPATCH_NONE;
    }
    case ui::ET_KEY_RELEASED:
      return POST_DISPATCH_NONE;
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
