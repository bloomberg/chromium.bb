// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/webvr_browser_test.h"

#include "build/build_config.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

// Tests that we can recover from a crash/disconnect on the DeviceService
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestDeviceServiceDisconnect) {
  t->LoadUrlAndAwaitInitialization(
      t->GetFileUrlForHtmlTestFile("test_isolated_device_service_disconnect"));

  // We expect one change from the initial device being available.
  t->PollJavaScriptBooleanOrFail("deviceChanges === 1",
                                 WebXrVrBrowserTestBase::kPollTimeoutMedium);

  t->EnterSessionWithUserGestureOrFail();
  MockXRDeviceHookBase device_hook;
  device_hook.TerminateDeviceServiceProcessForTesting();

  // Ensure that we've actually exited the session.
  t->PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession === null",
      WebXrVrBrowserTestBase::kPollTimeoutLong);

  // We need to create a mock here because otherwise WMR opens the real Mixed
  // Reality Portal if it's installed. The killing of the process unsets the
  // hook created by the one above, and the presence of the hook is what WMR
  // uses to determine whether it should uses the mock or real implementation.
  MockXRDeviceHookBase another_hook;

  // We expect one change indicating the device was disconnected, and then
  // one more indicating that the device was re-connected.
  t->PollJavaScriptBooleanOrFail("deviceChanges === 3",
                                 WebXrVrBrowserTestBase::kPollTimeoutMedium);

  // One last check now that we have the device change that we can actually
  // still enter an immersive session.
  t->EnterSessionWithUserGestureOrFail();
}
}  // namespace vr
