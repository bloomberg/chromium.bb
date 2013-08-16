// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remote_desktop_browsertest.h"

namespace remoting {

IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest, MANUAL_Launch) {
  VerifyInternetAccess();

  if (!NoInstall()) {
    VerifyChromotingLoaded(false);
    InstallChromotingApp();
  }

  VerifyChromotingLoaded(true);

  LaunchChromotingApp();

  // TODO: Remove this hack by blocking on the appropriate notification.
  // The browser may still be loading images embedded in the webapp. If we
  // uinstall it now those load will fail. Navigating away to avoid the load
  // failures.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  if (!NoCleanup()) {
    UninstallChromotingApp();
    VerifyChromotingLoaded(false);
  }
}

}  // namespace remoting
