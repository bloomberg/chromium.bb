// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_MAC_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_MAC_H_

#include "ui/events/event_handler.h"
#include "ui/views/controls/menu/menu_pre_target_handler.h"
#include "ui/views/event_monitor_mac.h"

namespace views {

class MenuPreTargetHandlerMac : public MenuPreTargetHandler,
                                public ui::EventHandler {
 public:
  MenuPreTargetHandlerMac(MenuController* controller, Widget* widget);
  ~MenuPreTargetHandlerMac() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  MenuController* controller_;  // Weak. Owns |this|.
  std::unique_ptr<EventMonitorMac> event_monitor_;

  DISALLOW_COPY_AND_ASSIGN(MenuPreTargetHandlerMac);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_MAC_H_
