// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_event_dispatcher_linux.h"

#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/menu/menu_controller.h"

namespace views {
namespace internal {

MenuEventDispatcher::MenuEventDispatcher(MenuController* controller)
    : menu_controller_(controller) {}

MenuEventDispatcher::~MenuEventDispatcher() {}

bool MenuEventDispatcher::CanDispatchEvent(const ui::PlatformEvent& event) {
  return true;
}

uint32_t MenuEventDispatcher::DispatchEvent(const ui::PlatformEvent& event) {
  bool should_quit = false;
  bool should_perform_default = true;
  if (menu_controller_->exit_type() == MenuController::EXIT_ALL ||
      menu_controller_->exit_type() == MenuController::EXIT_DESTROYED) {
    should_quit = true;
  } else {
    switch (ui::EventTypeFromNative(event)) {
      case ui::ET_KEY_PRESSED: {
        if (!menu_controller_->OnKeyDown(ui::KeyboardCodeFromNative(event))) {
          should_quit = true;
          should_perform_default = false;
          break;
        }

        // Do not check mnemonics if the Alt or Ctrl modifiers are pressed.
        int flags = ui::EventFlagsFromNative(event);
        if ((flags & (ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)) == 0) {
          char c = ui::GetCharacterFromKeyCode(
              ui::KeyboardCodeFromNative(event), flags);
          if (menu_controller_->SelectByChar(c)) {
            should_quit = true;
            should_perform_default = false;
            break;
          }
        }
        should_quit = false;
        should_perform_default = false;
        break;
      }
      case ui::ET_KEY_RELEASED:
        should_quit = false;
        should_perform_default = false;
        break;
      default:
        break;
    }
  }

  if (should_quit || menu_controller_->exit_type() != MenuController::EXIT_NONE)
    menu_controller_->TerminateNestedMessageLoop();

  return should_perform_default ? ui::POST_DISPATCH_PERFORM_DEFAULT
                                : ui::POST_DISPATCH_NONE;
}

}  // namespace internal
}  // namespace views
