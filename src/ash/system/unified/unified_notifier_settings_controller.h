// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_NOTIFIER_SETTINGS_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_NOTIFIER_SETTINGS_CONTROLLER_H_

#include <memory>

#include "ash/system/unified/detailed_view_controller.h"
#include "base/macros.h"

namespace ash {

class DetailedViewDelegate;
class UnifiedSystemTrayController;

// Controller of notifier settings detailed view in UnifiedSystemTray.
class UnifiedNotifierSettingsController : public DetailedViewController {
 public:
  explicit UnifiedNotifierSettingsController(
      UnifiedSystemTrayController* tray_controller);
  ~UnifiedNotifierSettingsController() override;

  // DetailedViewControllerBase:
  views::View* CreateView() override;
  base::string16 GetAccessibleName() const override;

 private:
  const std::unique_ptr<DetailedViewDelegate> detailed_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedNotifierSettingsController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_NOTIFIER_SETTINGS_CONTROLLER_H_
