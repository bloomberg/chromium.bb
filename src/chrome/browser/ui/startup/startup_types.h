// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_STARTUP_TYPES_H_
#define CHROME_BROWSER_UI_STARTUP_STARTUP_TYPES_H_

namespace chrome {
namespace startup {

enum IsProcessStartup {
  IS_NOT_PROCESS_STARTUP,   // Session is being created when a Chrome process is
                            // already running, e.g. clicking on a taskbar icon
                            // when Chrome is already running, or restoring a
                            // profile.
  IS_PROCESS_STARTUP        // Session is being created when the Chrome process
                            // is not already running.
};

enum IsFirstRun {
  IS_NOT_FIRST_RUN,         // Session is being created after Chrome has already
                            // been run at least once on the system.
  IS_FIRST_RUN              // Session is being created immediately after Chrome
                            // has been installed on the system.
};

}  // namespace startup
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_STARTUP_STARTUP_TYPES_H_
