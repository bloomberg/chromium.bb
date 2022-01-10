// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of accessibility feature pod button.
class ASH_EXPORT AccessibilityFeaturePodController
    : public FeaturePodControllerBase {
 public:
  AccessibilityFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  AccessibilityFeaturePodController(const AccessibilityFeaturePodController&) =
      delete;
  AccessibilityFeaturePodController& operator=(
      const AccessibilityFeaturePodController&) = delete;

  ~AccessibilityFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

 private:
  // Unowned.
  UnifiedSystemTrayController* const tray_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_FEATURE_POD_CONTROLLER_H_
