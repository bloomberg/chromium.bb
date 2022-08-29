// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_FEATURE_POD_CONTROLLER_H_

#include <string>

#include "ash/system/model/clock_observer.h"
#include "ash/system/unified/feature_pod_controller_base.h"

namespace ash {

class UnifiedSystemTrayController;

// Controller of a feature pod button that toggles night light mode.
class ASH_EXPORT NightLightFeaturePodController
    : public FeaturePodControllerBase,
      public ClockObserver {
 public:
  explicit NightLightFeaturePodController(
      UnifiedSystemTrayController* tray_controller);

  NightLightFeaturePodController(const NightLightFeaturePodController&) =
      delete;
  NightLightFeaturePodController& operator=(
      const NightLightFeaturePodController&) = delete;

  ~NightLightFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

  // ClockObserver:
  void OnDateFormatChanged() override;
  void OnSystemClockTimeUpdated() override;
  void OnSystemClockCanSetTimeChanged(bool can_set_time) override;
  void Refresh() override;

 private:
  // Returns the desired sub label of the feature pod button based on the
  // current status and schedule type of night light.
  const std::u16string GetPodSubLabel();

  // Updates the toggle state, sub label, and icon tooltip of the `button_`.
  void UpdateButton();

  UnifiedSystemTrayController* const tray_controller_;

  FeaturePodButton* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NIGHT_LIGHT_NIGHT_LIGHT_FEATURE_POD_CONTROLLER_H_
