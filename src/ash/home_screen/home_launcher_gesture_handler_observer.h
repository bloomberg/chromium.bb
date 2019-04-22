// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_LAUNCHER_GESTURE_HANDLER_OBSERVER_H_
#define ASH_HOME_SCREEN_HOME_LAUNCHER_GESTURE_HANDLER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ash {

class HomeLauncherGestureHandlerObserver : public base::CheckedObserver {
 public:
  // Called when the HomeLauncher has started to be dragged, or a positional
  // animation has begun.
  virtual void OnHomeLauncherTargetPositionChanged(bool showing,
                                                   int64_t display_id) {}

  // Called when the HomeLauncher positional animation has completed.
  virtual void OnHomeLauncherAnimationComplete(bool shown, int64_t display_id) {
  }
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_HOME_LAUNCHER_GESTURE_HANDLER_OBSERVER_H_
