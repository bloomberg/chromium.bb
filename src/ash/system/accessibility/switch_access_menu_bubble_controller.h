// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_MENU_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_MENU_BUBBLE_CONTROLLER_H_

#include "ash/system/tray/tray_bubble_view.h"

namespace ash {

class SwitchAccessBackButtonBubbleController;
class SwitchAccessMenuView;

// Manages the Switch Access menu bubble.
class ASH_EXPORT SwitchAccessMenuBubbleController
    : public TrayBubbleView::Delegate {
 public:
  SwitchAccessMenuBubbleController();
  ~SwitchAccessMenuBubbleController() override;

  SwitchAccessMenuBubbleController(const SwitchAccessMenuBubbleController&) =
      delete;
  SwitchAccessMenuBubbleController& operator=(
      const SwitchAccessMenuBubbleController&) = delete;

  void ShowBackButton(const gfx::Rect& anchor);
  void HideBackButton();

  void ShowMenu(const gfx::Rect& anchor,
                const std::vector<std::string>& actions_to_show);
  void HideMenuBubble();

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;

 private:
  friend class SwitchAccessMenuBubbleControllerTest;

  void ShowBackButtonForMenu();

  std::unique_ptr<SwitchAccessBackButtonBubbleController>
      back_button_controller_;

  // Owned by views hierarchy.
  SwitchAccessMenuView* menu_view_ = nullptr;
  TrayBubbleView* bubble_view_ = nullptr;

  views::Widget* widget_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_MENU_BUBBLE_CONTROLLER_H_
