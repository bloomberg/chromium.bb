// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_MONITOR_CHROMEOS_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_MONITOR_CHROMEOS_H_

#include "content/browser/memory/memory_monitor.h"
#include "content/common/content_export.h"

namespace content {

// A memory monitor for the ChromeOS platform.
class CONTENT_EXPORT MemoryMonitorChromeOS : public MemoryMonitor {
 public:
  MemoryMonitorChromeOS(MemoryMonitorDelegate* delegate);
  ~MemoryMonitorChromeOS() override;

  // MemoryMonitor:
  int GetFreeMemoryUntilCriticalMB() override;

  // Factory function to create an instance of this class.
  static std::unique_ptr<MemoryMonitorChromeOS> Create(
      MemoryMonitorDelegate* delegate);

 private:
  // The delegate to be used for retrieving system memory information. Used as a
  // testing seam.
  MemoryMonitorDelegate* delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_MONITOR_CHROMEOS_H_
