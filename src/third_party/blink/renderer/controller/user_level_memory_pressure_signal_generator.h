// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_

#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/controller/memory_usage_monitor.h"
#include "third_party/blink/renderer/platform/scheduler/public/rail_mode_observer.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

namespace user_level_memory_pressure_signal_generator_test {
class MockUserLevelMemoryPressureSignalGenerator;
}  // namespace user_level_memory_pressure_signal_generator_test

// Generates extra memory pressure signals (on top of the OS generated ones)
// when the memory usage goes over a threshold.
class CONTROLLER_EXPORT UserLevelMemoryPressureSignalGenerator
    : public RAILModeObserver,
      public MemoryUsageMonitor::Observer {
  USING_FAST_MALLOC(UserLevelMemoryPressureSignalGenerator);

 public:
  // Returns the shared instance.
  static UserLevelMemoryPressureSignalGenerator& Instance();

  UserLevelMemoryPressureSignalGenerator();
  ~UserLevelMemoryPressureSignalGenerator() override;

 private:
  friend class user_level_memory_pressure_signal_generator_test::
      MockUserLevelMemoryPressureSignalGenerator;

  // This is only virtual to override in tests.
  virtual void Generate(MemoryUsage);

  // Called by delayed_report_timer_ to report metrics.
  void OnTimerFired(TimerBase*);

  // RAILModeObserver:
  void OnRAILModeChanged(RAILMode rail_mode) override;

  // MemoryUsageMonitor::Observer:
  void OnMemoryPing(MemoryUsage) override;

  bool monitoring_ = false;
  bool is_loading_ = false;
  WTF::TimeTicks last_generated_;
  double memory_threshold_mb_;
  WTF::TimeDelta minimum_interval_;
  TaskRunnerTimer<UserLevelMemoryPressureSignalGenerator> delayed_report_timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_USER_LEVEL_MEMORY_PRESSURE_SIGNAL_GENERATOR_H_
