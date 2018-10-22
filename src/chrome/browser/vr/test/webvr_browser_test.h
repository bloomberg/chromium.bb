// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_WEBVR_BROWSER_TEST_H_
#define CHROME_BROWSER_VR_TEST_WEBVR_BROWSER_TEST_H_

#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace vr {

// WebVR-specific test base class.
class WebVrBrowserTestBase : public WebXrVrBrowserTestBase {
 public:
  bool XrDeviceFound(content::WebContents* web_contents) override;
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

// Test class with standard features enabled: WebVR, OpenVR support, and the
// Gamepad API.
class WebVrBrowserTestStandard : public WebVrBrowserTestBase {
 public:
  WebVrBrowserTestStandard() {
    append_switches_.push_back(switches::kEnableWebVR);
    enable_features_.push_back(features::kOpenVR);
    enable_features_.push_back(features::kGamepadExtensions);
  }
};

// Test class with WebVR disabled.
class WebVrBrowserTestWebVrDisabled : public WebVrBrowserTestBase {
 public:
  WebVrBrowserTestWebVrDisabled() {
    enable_features_.push_back(features::kOpenVR);
    enable_features_.push_back(features::kGamepadExtensions);
  }
};

// Test class with OpenVR support disabled.
class WebVrBrowserTestOpenVrDisabled : public WebVrBrowserTestBase {
 public:
  WebVrBrowserTestOpenVrDisabled() {
    append_switches_.push_back(switches::kEnableWebVR);
    enable_features_.push_back(features::kGamepadExtensions);
  }
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_WEBVR_BROWSER_TEST_H_
