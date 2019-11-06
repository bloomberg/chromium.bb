// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_SCREEN_PRESENTER_H_
#define ASH_HOME_SCREEN_HOME_SCREEN_PRESENTER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

class HomeScreenController;

// Helper class to schedule Home Screen view animations.
class ASH_EXPORT HomeScreenPresenter {
 public:
  explicit HomeScreenPresenter(HomeScreenController* controller);
  ~HomeScreenPresenter();

  // Schedules animation for the home screen when overview mode starts or ends.
  void ScheduleOverviewModeAnimation(bool start, bool animate);

 private:
  class OverviewAnimationMetricsReporter;

  HomeScreenController* controller_;

  // Metric reporter for entering/exiting overview.
  const std::unique_ptr<OverviewAnimationMetricsReporter>
      overview_animation_metrics_reporter_;

  DISALLOW_COPY_AND_ASSIGN(HomeScreenPresenter);
};

}  // namespace ash

#endif  //  ASH_HOME_SCREEN_HOME_SCREEN_PRESENTER_H_
