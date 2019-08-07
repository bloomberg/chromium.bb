// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STARTUP_DATA_IMPL_H_
#define CONTENT_BROWSER_STARTUP_DATA_IMPL_H_

#include <memory>

#include "base/power_monitor/power_monitor.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/common/content_export.h"
#include "content/public/browser/startup_data.h"

namespace content {

class ServiceManagerContext;

// The browser implementation of StartupData.
struct CONTENT_EXPORT StartupDataImpl : public StartupData {
  StartupDataImpl();
  ~StartupDataImpl() override;

  std::unique_ptr<BrowserProcessSubThread> thread;
  ServiceManagerContext* service_manager_context;
  std::unique_ptr<base::PowerMonitor> power_monitor;
};

}  // namespace content

#endif  // CONTENT_BROWSER_STARTUP_DATA_IMPL_H_
