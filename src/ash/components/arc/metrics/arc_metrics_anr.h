// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_METRICS_ARC_METRICS_ANR_H_
#define ASH_COMPONENTS_ARC_METRICS_ARC_METRICS_ANR_H_

#include "ash/components/arc/mojom/anr.mojom.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"

class PrefService;

namespace arc {

// Handles ANR events and supports UMA metrics.
class ArcMetricsAnr {
 public:
  explicit ArcMetricsAnr(PrefService* prefs);

  ArcMetricsAnr(const ArcMetricsAnr&) = delete;
  ArcMetricsAnr& operator=(const ArcMetricsAnr&) = delete;

  ~ArcMetricsAnr();

  void Report(mojom::AnrPtr anr);

 private:
  void LogOnStart();
  void UpdateRate();
  void SetLogOnStartPending();

  // ANR count for first 10 minitues after start.
  int count_10min_after_start_ = 0;
  // Set to true in case |count_10min_after_start_| is pending and has to be
  // persisted on restart. Once ANR count on start is reported this is set to
  // false.
  bool log_on_start_pending_ = false;
  base::OneShotTimer start_timer_;
  base::OneShotTimer pending_start_timer_;
  base::RepeatingTimer period_updater_;
  PrefService* const prefs_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_METRICS_ARC_METRICS_ANR_H_
