// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_EVENT_DISPATCHER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_EVENT_DISPATCHER_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/events/platform/platform_event_dispatcher.h"

namespace views {

class MenuController;

namespace internal {

// A message-pump dispatcher object used to dispatch events from the nested
// message-loop initiated by the MenuController.
class MenuEventDispatcher : public ui::PlatformEventDispatcher {
 public:
  explicit MenuEventDispatcher(MenuController* menu_controller);
  ~MenuEventDispatcher() override;

 private:
  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  MenuController* menu_controller_;

  DISALLOW_COPY_AND_ASSIGN(MenuEventDispatcher);
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_EVENT_DISPATCHER_H_
