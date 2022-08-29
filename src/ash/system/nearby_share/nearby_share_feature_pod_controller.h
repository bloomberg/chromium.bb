// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_FEATURE_POD_CONTROLLER_H_
#define ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_FEATURE_POD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/nearby_share/nearby_share_controller_impl.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

class NearbyShareDelegate;
class UnifiedSystemTrayController;

// Controller for a feature pod button that toggles the high visibility mode of
// Nearby Share.
class ASH_EXPORT NearbyShareFeaturePodController
    : public FeaturePodControllerBase,
      public NearbyShareControllerImpl::Observer {
 public:
  explicit NearbyShareFeaturePodController(
      UnifiedSystemTrayController* tray_controller);
  NearbyShareFeaturePodController(const NearbyShareFeaturePodController&) =
      delete;
  NearbyShareFeaturePodController& operator=(
      const NearbyShareFeaturePodController&) = delete;
  ~NearbyShareFeaturePodController() override;

  // FeaturePodControllerBase:
  FeaturePodButton* CreateButton() override;
  void OnIconPressed() override;
  void OnLabelPressed() override;
  SystemTrayItemUmaType GetUmaType() const override;

  // NearbyShareController::Observer
  void OnHighVisibilityEnabledChanged(bool enabled) override;

 private:
  void UpdateButton(bool enabled);

  base::TimeDelta RemainingHighVisibilityTime() const;

  // Countdown timer fires periodically to update the remaining time until
  // |shutoff_time_| as displayed by the pod button sub-label.
  base::RepeatingTimer countdown_timer_;
  base::TimeTicks shutoff_time_;

  UnifiedSystemTrayController* const tray_controller_;
  NearbyShareDelegate* const nearby_share_delegate_;
  NearbyShareControllerImpl* const nearby_share_controller_;
  FeaturePodButton* button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_FEATURE_POD_CONTROLLER_H_
