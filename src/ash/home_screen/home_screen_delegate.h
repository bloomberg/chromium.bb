// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_SCREEN_DELEGATE_H_
#define ASH_HOME_SCREEN_HOME_SCREEN_DELEGATE_H_

#include "base/callback.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

// Delegate for implementation-specific home screen behavior.
class HomeScreenDelegate {
 public:
  // Callback which fills out the passed settings object, allowing the caller to
  // animate with the given settings.
  using UpdateAnimationSettingsCallback =
      base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings* settings,
                                   bool observe)>;

  virtual ~HomeScreenDelegate() = default;

  // Updates the y position and opacity of the home launcher view. If |callback|
  // is non-null, it should be called with animation settings.
  virtual void UpdateYPositionAndOpacityForHomeLauncher(
      int y_position_in_screen,
      float opacity,
      UpdateAnimationSettingsCallback callback) = 0;

  // Updates the home launcher view after its show animation has completed.
  virtual void UpdateAfterHomeLauncherShown() = 0;

  // Returns an optional animation duration which is going to be used to set
  // the transition animation if provided.
  virtual base::Optional<base::TimeDelta> GetOptionalAnimationDuration() = 0;
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_HOME_SCREEN_DELEGATE_H_
