// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/webvr_browser_test.h"

#include "build/build_config.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

// TODO(https://crbug.com/926048): Figure out why/fix the WMR version of this
// causes the real Mixed Reality Portal to open if it's installed.
// Tests that we can recover from a crash/disconnect on the DeviceService
IN_PROC_BROWSER_TEST_F(WebXrVrOpenVrBrowserTest, TestDeviceServiceDisconnect) {
  LoadUrlAndAwaitInitialization(
      GetFileUrlForHtmlTestFile("test_isolated_device_service_disconnect"));

  // We expect one change from the initial device being available.
  PollJavaScriptBooleanOrFail("deviceChanges === 1", kPollTimeoutMedium);

  EnterSessionWithUserGestureOrFail();
  MockXRDeviceHookBase device_hook;
  device_hook.TerminateDeviceServiceProcessForTesting();

  // Ensure that we've actually exited the session.
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession === null",
      kPollTimeoutLong);

  // We expect one change indicating the device was disconnected, and then
  // one more indicating that the device was re-connected.
  PollJavaScriptBooleanOrFail("deviceChanges === 3", kPollTimeoutMedium);

  // One last check now that we have the device change that we can actually
  // still enter an immersive session.
  EnterSessionWithUserGestureOrFail();
}
}  // namespace vr
