// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_BUBBLE_CONTROLLER_H_

#include "ash/system/accessibility/autoclick_menu_view.h"
#include "ash/system/tray/tray_bubble_view.h"

namespace ash {

// Manages the bubble which contains an AutoclickMenuView.
class AutoclickMenuBubbleController : public TrayBubbleView::Delegate {
 public:
  AutoclickMenuBubbleController();
  ~AutoclickMenuBubbleController() override;

  // Sets the currently selected event type.
  void SetEventType(mojom::AutoclickEventType type);

  // Sets the menu's position on the screen.
  void SetPosition(mojom::AutoclickMenuPosition position);

  void ShowBubble(mojom::AutoclickEventType event_type,
                  mojom::AutoclickMenuPosition position);

  void CloseBubble();

  // Performs the mouse events on the bubble. at the given location in DIPs.
  void ClickOnBubble(gfx::Point location_in_dips, int mouse_event_flags);

  // Whether the tray button or the bubble, if the bubble exists, contain
  // the given screen point.
  bool ContainsPointInScreen(const gfx::Point& point);

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;

 private:
  friend class AutoclickMenuBubbleControllerTest;
  friend class AutoclickTest;

  // Owned by views hierarchy.
  AutoclickMenuBubbleView* bubble_view_ = nullptr;
  AutoclickMenuView* menu_view_ = nullptr;

  views::Widget* bubble_widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AutoclickMenuBubbleController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_BUBBLE_CONTROLLER_H_
