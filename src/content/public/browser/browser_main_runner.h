// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_MAIN_RUNNER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_MAIN_RUNNER_H_

#include <memory>

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

struct MainFunctionParams;

// This class is responsible for browser initialization, running and shutdown.
class CONTENT_EXPORT BrowserMainRunner {
 public:
  virtual ~BrowserMainRunner() {}

  // Create a new BrowserMainRunner object.
  static std::unique_ptr<BrowserMainRunner> Create();

  // Returns true if the BrowserMainRunner has exited the main loop.
  static bool ExitedMainMessageLoop();

  // Initialize all necessary browser state. The |parameters| values will be
  // copied. Returning a non-negative value indicates that initialization
  // failed, and the returned value is used as the exit code for the process.
  virtual int Initialize(const content::MainFunctionParams& parameters) = 0;

#if defined(OS_ANDROID)
  // Run all queued startup tasks. Only defined on Android because other
  // platforms run startup tasks immediately.
  virtual void SynchronouslyFlushStartupTasks() = 0;
#endif  // OS_ANDROID

  // Perform the default run logic.
  virtual int Run() = 0;

  // Shut down the browser state.
  virtual void Shutdown() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_MAIN_RUNNER_H_
