// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_UNIFIED_IME_DETAILED_VIEW_CONTROLLER_H_
#define ASH_SYSTEM_IME_UNIFIED_IME_DETAILED_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_observer.h"
#include "base/macros.h"

namespace ash {
namespace tray {
class IMEDetailedView;
}  // namespace tray

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of IME detailed view in UnifiedSystemTray.
class UnifiedIMEDetailedViewController : public DetailedViewController,
                                         public VirtualKeyboardObserver,
                                         public AccessibilityObserver,
                                         public IMEObserver {
 public:
  explicit UnifiedIMEDetailedViewController(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedIMEDetailedViewController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;
  base::string16 GetAccessibleName() const override;

  // VirtualKeyboardObserver:
  void OnKeyboardSuppressionChanged(bool suppressed) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // IMEObserver:
  void OnIMERefresh() override;
  void OnIMEMenuActivationChanged(bool is_active) override;

 private:
  void Update();

  bool ShouldShowKeyboardToggle() const;

  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  tray::IMEDetailedView* view_ = nullptr;

  bool keyboard_suppressed_ = false;

  DISALLOW_COPY_AND_ASSIGN(UnifiedIMEDetailedViewController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_UNIFIED_IME_DETAILED_VIEW_CONTROLLER_H_
