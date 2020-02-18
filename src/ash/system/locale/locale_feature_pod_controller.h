// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCALE_LOCALE_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_LOCALE_LOCALE_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/macros.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of locale feature pod button.
class ASH_EXPORT LocaleFeaturePodController : public FeaturePodControllerBase {
 public:
  explicit LocaleFeaturePodController(
      UnifiedSystemTrayController* tray_controller);
  ~LocaleFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

 private:
  // Unowned.
  UnifiedSystemTrayController* const tray_controller_;

  DISALLOW_COPY_AND_ASSIGN(LocaleFeaturePodController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCALE_LOCALE_FEATURE_POD_CONTROLLER_H_
