// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_CONTROLLER_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/system/accessibility/floating_accessibility_detailed_controller.h"
#include "ash/system/accessibility/floating_accessibility_view.h"
#include "ash/system/locale/locale_update_controller_impl.h"

namespace ash {

class AccessibilityControllerImpl;
class FloatingAccessibilityView;

// Controls the floating accessibility menu.
class ASH_EXPORT FloatingAccessibilityController
    : public FloatingAccessibilityView::Delegate,
      public FloatingAccessibilityDetailedController::Delegate,
      public TrayBubbleView::Delegate,
      public LocaleChangeObserver,
      public AccessibilityObserver {
 public:
  explicit FloatingAccessibilityController(
      AccessibilityControllerImpl* accessibility_controller);
  FloatingAccessibilityController(const FloatingAccessibilityController&) =
      delete;
  FloatingAccessibilityController& operator=(
      const FloatingAccessibilityController&) = delete;
  ~FloatingAccessibilityController() override;

  // Starts showing the floating menu when called.
  void Show(FloatingMenuPosition position);
  void SetMenuPosition(FloatingMenuPosition new_position);

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // Focuses on the first element in the floating menu.
  void FocusOnMenu();

 private:
  friend class FloatingAccessibilityControllerTest;
  // FloatingAccessibilityView::Delegate:
  void OnDetailedMenuEnabled(bool enabled) override;
  void OnLayoutChanged() override;
  // FloatingAccessibilityDetailedController::Delegate:
  void OnDetailedMenuClosed() override;
  views::Widget* GetBubbleWidget() override;
  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;
  base::string16 GetAccessibleNameForBubble() override;
  // LocaleChangeObserver:
  void OnLocaleChanged() override;

  FloatingAccessibilityView* menu_view_ = nullptr;
  FloatingAccessibilityBubbleView* bubble_view_ = nullptr;
  views::Widget* bubble_widget_ = nullptr;

  bool detailed_view_shown_ = false;

  // Controller for the detailed view, exists only when visible.
  std::unique_ptr<FloatingAccessibilityDetailedController>
      detailed_menu_controller_;

  FloatingMenuPosition position_ = kDefaultFloatingMenuPosition;

  // Used in tests to notify on the menu layout change events.
  base::RepeatingClosure on_layout_change_;

  AccessibilityControllerImpl* const accessibility_controller_;  // Owns us.
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_CONTROLLER_H_
