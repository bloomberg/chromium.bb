// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_USAGE_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_USAGE_MONITOR_H_

#include "base/observer_list.h"
#include "third_party/blink/renderer/controller/controller_export.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

// nan means data not available.
struct MemoryUsage {
  double v8_bytes = std::numeric_limits<double>::quiet_NaN();
  double partition_alloc_bytes = std::numeric_limits<double>::quiet_NaN();
  double blink_gc_bytes = std::numeric_limits<double>::quiet_NaN();
  double private_footprint_bytes = std::numeric_limits<double>::quiet_NaN();
  double swap_bytes = std::numeric_limits<double>::quiet_NaN();
  double vm_size_bytes = std::numeric_limits<double>::quiet_NaN();
};

// Periodically checks the memory usage and notifies its observers. Monitoring
// automatically starts/stops depending on whether an observer exists.
class CONTROLLER_EXPORT MemoryUsageMonitor {
 public:
  static MemoryUsageMonitor& Instance();

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnMemoryPing(MemoryUsage) = 0;
  };

  MemoryUsageMonitor();
  virtual ~MemoryUsageMonitor() = default;

  // Returns the current memory usage.
  MemoryUsage GetCurrentMemoryUsage();

  // Ensures that an observer is only added once.
  void AddObserver(Observer*);
  // Observers must be removed before they are destroyed.
  void RemoveObserver(Observer*);

  bool TimerIsActive() const { return timer_.IsActive(); }

 protected:
  // Adds V8 related memory usage data to the given struct.
  void GetV8MemoryUsage(MemoryUsage&);
  // Adds Blink related memory usage data to the given struct.
  void GetBlinkMemoryUsage(MemoryUsage&);
  // Adds process related memory usage data to the given struct.
  virtual void GetProcessMemoryUsage(MemoryUsage&) {}

 private:
  virtual void StartMonitoringIfNeeded();
  virtual void StopMonitoring();

  void TimerFired(TimerBase*);

  TaskRunnerTimer<MemoryUsageMonitor> timer_;
  base::ObserverList<Observer> observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_MEMORY_USAGE_MONITOR_H_
