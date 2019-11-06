// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_H_

#include <memory>

#include "base/macros.h"

namespace views {

class MenuController;
class Widget;

// A MenuPreTargetHandler is responsible for intercepting events destined for
// another widget (the menu's owning widget) and letting the menu's controller
// try dispatching them first.
class MenuPreTargetHandler {
 public:
  virtual ~MenuPreTargetHandler() = default;

  // There are separate implementations of this method for Aura platforms and
  // for Mac.
  static std::unique_ptr<MenuPreTargetHandler> Create(
      MenuController* controller,
      Widget* owner);

 protected:
  MenuPreTargetHandler() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuPreTargetHandler);
};

}  // namespace views

#endif  //  UI_VIEWS_CONTROLS_MENU_MENU_PRE_TARGET_HANDLER_H_
