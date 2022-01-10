// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_DETECTOR_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_DETECTOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/idle_detector.h"

class PrefRegistrySimple;

namespace ash {

// Helper for idle state and demo-mode detection.
// Should be initialized on OOBE start.
class DemoModeDetector {
 public:
  // Interface for notification that device stayed in demo mode long enough
  // to trigger Demo mode.
  class Observer {
   public:
    virtual ~Observer() = default;
  };

  DemoModeDetector(const base::TickClock* clock, Observer* observer);

  DemoModeDetector(const DemoModeDetector&) = delete;
  DemoModeDetector& operator=(const DemoModeDetector&) = delete;

  virtual ~DemoModeDetector();

  // Registers the preference for derelict state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  static const base::TimeDelta kDerelictDetectionTimeout;
  static const base::TimeDelta kDerelictIdleTimeout;
  static const base::TimeDelta kOobeTimerUpdateInterval;

 private:
  void InitDetection();
  void StartIdleDetection();
  void StartOobeTimer();
  void OnIdle();
  void OnOobeTimerUpdate();
  void SetupTimeouts();
  bool IsDerelict();

  Observer* observer_;

  // Total time this machine has spent on OOBE.
  base::TimeDelta time_on_oobe_;

  std::unique_ptr<IdleDetector> idle_detector_;

  base::RepeatingTimer oobe_timer_;

  // Timeout to detect if the machine is in a derelict state.
  base::TimeDelta derelict_detection_timeout_;

  // Timeout before showing our demo app if the machine is in a derelict state.
  base::TimeDelta derelict_idle_timeout_;

  // Time between updating our total time on oobe.
  base::TimeDelta oobe_timer_update_interval_;

  bool demo_launched_ = false;

  const base::TickClock* tick_clock_;

  base::WeakPtrFactory<DemoModeDetector> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::DemoModeDetector;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_DETECTOR_H_
