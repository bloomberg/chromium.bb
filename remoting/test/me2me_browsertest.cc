// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/remote_desktop_browsertest.h"
#include "remoting/test/waiter.h"

namespace remoting {

class Me2MeBrowserTest : public RemoteDesktopBrowserTest {
 protected:
  void TestKeyboardInput();
  void TestMouseInput();
};

IN_PROC_BROWSER_TEST_F(Me2MeBrowserTest,
                       MANUAL_Me2Me_Connect_Localhost) {
  VerifyInternetAccess();

  Install();

  LaunchChromotingApp();

  // Authorize, Authenticate, and Approve.
  Auth();

  StartMe2Me();

  ConnectToLocalHost();

  TestKeyboardInput();

  Cleanup();
}

void Me2MeBrowserTest::TestKeyboardInput() {
  // Start a terminal windows with ctrl+alt+T
  SimulateKeyPressWithCode(ui::VKEY_T, "KeyT", true, false, true, false);
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(1)).Wait());

  // Run an arbitrary command so that I can verify the result visually.
  // TODO: Verify programatically the keyboard events are received by the host.
  SimulateStringInput("ls -la\n");
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(1)).Wait());
  SimulateStringInput("exit\n");
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(1)).Wait());
}

void Me2MeBrowserTest::TestMouseInput() {
  SimulateMouseLeftClickAt(10, 50);
  // TODO: Verify programatically the mouse events are received by the host.
  // This will be tricky as it depends on the host OS, window manager, desktop
  // layout, and screen resolution. Until then we need to visually verify that
  // "Dash Home" is clicked on a Unity window manager.
  ASSERT_TRUE(TimeoutWaiter(base::TimeDelta::FromSeconds(5)).Wait());
}

}  // namespace remoting
