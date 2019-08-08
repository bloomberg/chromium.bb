// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_pressure_monitor.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "chrome/browser/memory/memory_pressure_monitor_win.h"
#endif

namespace memory {
namespace {

// The global instance.
MemoryPressureMonitor* g_memory_pressure_monitor = nullptr;

}  // namespace

// static
std::unique_ptr<MemoryPressureMonitor> MemoryPressureMonitor::Create() {
#if defined(OS_WIN)
  return base::WrapUnique(new MemoryPressureMonitorWin());
#else
  NOTREACHED();
  return base::WrapUnique(new MemoryPressureMonitor());
#endif
}

MemoryPressureMonitor::MemoryPressureMonitor() {
  DCHECK(!g_memory_pressure_monitor);
  g_memory_pressure_monitor = this;
}

MemoryPressureMonitor::~MemoryPressureMonitor() {
  DCHECK_EQ(this, g_memory_pressure_monitor);
  g_memory_pressure_monitor = nullptr;
}

void MemoryPressureMonitor::OnMemoryPressureLevelChange(
    const base::MemoryPressureListener::MemoryPressureLevel previous_level,
    const base::MemoryPressureListener::MemoryPressureLevel new_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(sebmarchand): Implement this function, add the logic that will record
  // some metrics and dispatch the memory pressure level changes.
  NOTREACHED();
}

}  // namespace memory
