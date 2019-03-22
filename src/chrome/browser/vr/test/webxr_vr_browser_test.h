// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_
#define CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_

#include "chrome/browser/vr/test/webxr_browser_test.h"
#include "chrome/browser/vr/test/xr_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

namespace vr {

// WebXR for VR-specific test base class.
class WebXrVrBrowserTestBase : public WebXrBrowserTestBase {
 public:
  void EnterSessionWithUserGesture(content::WebContents* web_contents) override;
  void EnterSessionWithUserGestureOrFail(
      content::WebContents* web_contents) override;
  void EndSession(content::WebContents* web_contents) override;
  void EndSessionOrFail(content::WebContents* web_contents) override;

  // Necessary to use the WebContents-less versions of functions.
  using WebXrBrowserTestBase::XrDeviceFound;
  using WebXrBrowserTestBase::EnterSessionWithUserGesture;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureAndWait;
  using WebXrBrowserTestBase::EnterSessionWithUserGestureOrFail;
  using WebXrBrowserTestBase::EndSession;
  using WebXrBrowserTestBase::EndSessionOrFail;
};

// Test class with standard features enabled: WebXR and OpenVR.
class WebXrVrBrowserTestStandard : public WebXrVrBrowserTestBase {
 public:
  WebXrVrBrowserTestStandard() {
    enable_features_.push_back(features::kOpenVR);
    enable_features_.push_back(features::kWebXr);
  }
};

// Test class with WebXR disabled.
class WebXrVrBrowserTestWebXrDisabled : public WebXrVrBrowserTestBase {
 public:
  WebXrVrBrowserTestWebXrDisabled() {
    enable_features_.push_back(features::kOpenVR);
  }
};

// Test class with OpenVR disabled.
class WebXrVrBrowserTestOpenVrDisabled : public WebXrVrBrowserTestBase {
 public:
  WebXrVrBrowserTestOpenVrDisabled() {
    enable_features_.push_back(features::kWebXr);
  }
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_WEBXR_VR_BROWSER_TEST_H_
