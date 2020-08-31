// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_FEATURE_POD_CONTROLLER_H_

#include "ash/display/privacy_screen_controller.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/macros.h"

namespace ash {

// Controller of a feature pod button for toggling the built-in privacy screen.
class PrivacyScreenFeaturePodController
    : public FeaturePodControllerBase,
      public PrivacyScreenController::Observer {
 public:
  PrivacyScreenFeaturePodController();
  ~PrivacyScreenFeaturePodController() override;

  PrivacyScreenFeaturePodController(const PrivacyScreenFeaturePodController&) =
      delete;
  PrivacyScreenFeaturePodController& operator=(
      const PrivacyScreenFeaturePodController&) = delete;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

 private:
  void TogglePrivacyScreen();
  void UpdateButton();

  // PrivacyScreenController::Observer:
  void OnPrivacyScreenSettingChanged(bool enabled) override;

  // Unowned.
  FeaturePodButton* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_PRIVACY_SCREEN_FEATURE_POD_CONTROLLER_H_
