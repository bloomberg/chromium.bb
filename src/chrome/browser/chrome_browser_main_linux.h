// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains functions used by BrowserMain() that are linux-specific.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chrome_browser_main_posix.h"

class ChromeBrowserMainPartsLinux : public ChromeBrowserMainPartsPosix {
 public:
  ChromeBrowserMainPartsLinux(const content::MainFunctionParams& parameters,
                              StartupData* startup_data);

  ChromeBrowserMainPartsLinux(const ChromeBrowserMainPartsLinux&) = delete;
  ChromeBrowserMainPartsLinux& operator=(const ChromeBrowserMainPartsLinux&) =
      delete;

  ~ChromeBrowserMainPartsLinux() override;

  // ChromeBrowserMainParts overrides.
  void PreProfileInit() override;
  void PostCreateMainMessageLoop() override;
#if defined(USE_DBUS) && !defined(OS_CHROMEOS)
  // Only needed for native Linux, to set up the low-memory-monitor-based memory
  // monitoring (which depends on D-Bus).
  void PostBrowserStart() override;
#endif
  void PostDestroyThreads() override;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_LINUX_H_
