// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/webvr_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr/WebXrVrInputTest.java.
// End-to-end tests for user input interaction with WebXR/WebVR.

namespace vr {

// Test that focus is locked to the presenting display for the purposes of VR/XR
// input.
void TestPresentationLocksFocusImpl(WebXrVrBrowserTestBase* t,
                                    std::string filename) {
  t->LoadUrlAndAwaitInitialization(t->GetHtmlTestFile(filename));
  t->EnterSessionWithUserGestureOrFail();
  t->ExecuteStepAndWait("stepSetupFocusLoss()");
  t->EndTest();
}

IN_PROC_BROWSER_TEST_F(WebVrBrowserTestStandard,
                       REQUIRES_GPU(TestPresentationLocksFocus)) {
  TestPresentationLocksFocusImpl(this, "test_presentation_locks_focus");
}
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard,
                       REQUIRES_GPU(TestPresentationLocksFocus)) {
  TestPresentationLocksFocusImpl(this, "webxr_test_presentation_locks_focus");
}

}  // namespace vr
