// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_DESKTOP_TASK_SWITCH_METRIC_RECORDER_H_
#define ASH_METRICS_DESKTOP_TASK_SWITCH_METRIC_RECORDER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

// Tracks metrics for task switches caused by the user activating a task window
// by clicking or tapping on it.
class ASH_EXPORT DesktopTaskSwitchMetricRecorder
    : public ::wm::ActivationChangeObserver {
 public:
  DesktopTaskSwitchMetricRecorder();
  ~DesktopTaskSwitchMetricRecorder() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

 private:
  // Tracks the last active task window.
  aura::Window* last_active_task_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopTaskSwitchMetricRecorder);
};

}  // namespace ash

#endif  // ASH_METRICS_DESKTOP_TASK_SWITCH_METRIC_RECORDER_H_
