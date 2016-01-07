// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_key_event_handler.h"

#include "ui/aura/env.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/views_delegate.h"

namespace views {

namespace {

const int kKeyFlagsMask = ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN;

}  // namespace

MenuKeyEventHandler::MenuKeyEventHandler() {
  aura::Env::GetInstanceDontCreate()->PrependPreTargetHandler(this);
}

MenuKeyEventHandler::~MenuKeyEventHandler() {
  aura::Env::GetInstanceDontCreate()->RemovePreTargetHandler(this);
}

void MenuKeyEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  MenuController* menu_controller = MenuController::GetActiveInstance();
  DCHECK(menu_controller);

  if (menu_controller->exit_type() == MenuController::EXIT_ALL ||
      menu_controller->exit_type() == MenuController::EXIT_DESTROYED) {
    // If the event has arrived after the menu's exit type had changed but
    // before its message loop terminated, the event will continue its normal
    // propagation for the following reason:
    // If the user accepts a menu item in a nested menu, the menu item action is
    // run after the base::RunLoop for the innermost menu has quit but before
    // the base::RunLoop for the outermost menu has quit. If the menu item
    // action starts a base::RunLoop, the outermost menu's base::RunLoop will
    // not quit till the action's base::RunLoop ends. IDC_BOOKMARK_BAR_OPEN_ALL
    // sometimes opens a modal dialog. The modal dialog starts a base::RunLoop
    // and keeps the base::RunLoop running for the duration of its lifetime.
    menu_controller->TerminateNestedMessageLoopIfNecessary();
    return;
  }

  if (event->type() == ui::ET_KEY_PRESSED) {
    menu_controller->OnKeyDown(event->key_code());

    // Do not check mnemonics if the Alt or Ctrl modifiers are pressed. For
    // example Ctrl+<T> is an accelerator, but <T> only is a mnemonic.
    const int flags = event->flags();
    if (menu_controller->exit_type() == MenuController::EXIT_NONE &&
        (flags & kKeyFlagsMask) == 0) {
      char c = ui::DomCodeToUsLayoutCharacter(event->code(), flags);
      menu_controller->SelectByChar(c);
    }
  }

  if (!menu_controller->TerminateNestedMessageLoopIfNecessary()) {
    ui::Accelerator accelerator(*event);
    ViewsDelegate::ProcessMenuAcceleratorResult result =
        ViewsDelegate::GetInstance()->ProcessAcceleratorWhileMenuShowing(
            accelerator);
    if (result == ViewsDelegate::ProcessMenuAcceleratorResult::CLOSE_MENU)
      menu_controller->CancelAll();
  }

  event->StopPropagation();
}

}  // namespace views
