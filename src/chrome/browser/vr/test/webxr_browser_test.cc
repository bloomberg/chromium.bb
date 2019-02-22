// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/webxr_browser_test.h"
#include "content/public/browser/web_contents.h"

namespace vr {

bool WebXrBrowserTestBase::XrDeviceFound(content::WebContents* web_contents) {
  return RunJavaScriptAndExtractBoolOrFail("xrDevice != null", web_contents);
}

void WebXrBrowserTestBase::EnterSessionWithUserGestureAndWait(
    content::WebContents* web_contents) {
  EnterSessionWithUserGesture(web_contents);
  WaitOnJavaScriptStep(web_contents);
}

bool WebXrBrowserTestBase::XrDeviceFound() {
  return XrDeviceFound(GetFirstTabWebContents());
}

void WebXrBrowserTestBase::EnterSessionWithUserGesture() {
  EnterSessionWithUserGesture(GetFirstTabWebContents());
}

void WebXrBrowserTestBase::EnterSessionWithUserGestureAndWait() {
  EnterSessionWithUserGestureAndWait(GetFirstTabWebContents());
}

void WebXrBrowserTestBase::EnterSessionWithUserGestureOrFail() {
  EnterSessionWithUserGestureOrFail(GetFirstTabWebContents());
}

void WebXrBrowserTestBase::EndSession() {
  EndSession(GetFirstTabWebContents());
}

void WebXrBrowserTestBase::EndSessionOrFail() {
  EndSessionOrFail(GetFirstTabWebContents());
}

}  // namespace vr
