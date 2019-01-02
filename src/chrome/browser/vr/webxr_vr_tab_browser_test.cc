// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/vr/test/webvr_browser_test.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr/WebXrVrTabTest.java.
// End-to-end tests for testing WebXR/WebVR's interaction with multiple tabs.

namespace vr {

// Tests that non-focused tabs cannot get pose information from WebVR/WebXR.
void TestPoseDataUnfocusedTabImpl(WebXrVrBrowserTestBase* t,
                                  std::string filename) {
  t->LoadUrlAndAwaitInitialization(t->GetHtmlTestFile(filename));
  t->ExecuteStepAndWait("stepCheckFrameDataWhileFocusedTab()");
  chrome::AddTabAt(t->browser(), GURL(url::kAboutBlankURL),
                   -1 /* index, append to end */, true /* foreground */);
  t->ExecuteStepAndWait("stepCheckFrameDataWhileNonFocusedTab()");
  t->EndTest();
}

IN_PROC_BROWSER_TEST_F(WebVrBrowserTestStandard,
                       REQUIRES_GPU(TestPoseDataUnfocusedTab)) {
  TestPoseDataUnfocusedTabImpl(this, "test_pose_data_unfocused_tab");
}
IN_PROC_BROWSER_TEST_F(WebXrVrBrowserTestStandard,
                       REQUIRES_GPU(TestPoseDataUnfocusedTab)) {
  TestPoseDataUnfocusedTabImpl(this, "webxr_test_pose_data_unfocused_tab");
}

}  // namespace vr
