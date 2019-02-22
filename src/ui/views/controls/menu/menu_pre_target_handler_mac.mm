// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_pre_target_handler_mac.h"

#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"

namespace views {

MenuPreTargetHandlerMac::MenuPreTargetHandlerMac(MenuController* controller,
                                                 Widget* widget)
    : controller_(controller),
      event_monitor_(
          std::make_unique<EventMonitorMac>(this, widget->GetNativeWindow())) {}

MenuPreTargetHandlerMac::~MenuPreTargetHandlerMac() {}

void MenuPreTargetHandlerMac::OnKeyEvent(ui::KeyEvent* event) {
  if (controller_->OnWillDispatchKeyEvent(event) !=
      ui::POST_DISPATCH_PERFORM_DEFAULT) {
    event->SetHandled();
  }
}

// static
std::unique_ptr<MenuPreTargetHandler> MenuPreTargetHandler::Create(
    MenuController* controller,
    Widget* widget) {
  return std::make_unique<MenuPreTargetHandlerMac>(controller, widget);
}

}  // namespace views
