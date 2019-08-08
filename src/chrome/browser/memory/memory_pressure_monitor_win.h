// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_WIN_H_
#define CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_WIN_H_

#include "base/macros.h"
#include "chrome/browser/memory/memory_pressure_monitor.h"

namespace memory {

// Windows implementation of the memory pressure monitor.
class MemoryPressureMonitorWin : public MemoryPressureMonitor {
 public:
  ~MemoryPressureMonitorWin() override;

 protected:
  // This object is expected to be created via MemoryPressureMonitor::Create.
  friend class MemoryPressureMonitor;

  MemoryPressureMonitorWin();

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryPressureMonitorWin);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_WIN_H_
