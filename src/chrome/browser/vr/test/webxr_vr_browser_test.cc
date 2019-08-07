// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

void WebXrVrBrowserTestBase::EnterSessionWithUserGesture(
    content::WebContents* web_contents) {
  // ExecuteScript runs with a user gesture, so we can just directly call
  // requestSession instead of having to do the hacky workaround the
  // instrumentation tests use of actually sending a click event to the canvas.
  RunJavaScriptOrFail("onRequestSession()", web_contents);
}

void WebXrVrBrowserTestBase::EnterSessionWithUserGestureOrFail(
    content::WebContents* web_contents) {
  EnterSessionWithUserGesture(web_contents);
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession != null",
      kPollTimeoutLong, web_contents);
}

void WebXrVrBrowserTestBase::EndSession(content::WebContents* web_contents) {
  RunJavaScriptOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession.end()",
      web_contents);
}

void WebXrVrBrowserTestBase::EndSessionOrFail(
    content::WebContents* web_contents) {
  EndSession(web_contents);
  PollJavaScriptBooleanOrFail(
      "sessionInfos[sessionTypes.IMMERSIVE].currentSession == null",
      kPollTimeoutLong, web_contents);
}

}  // namespace vr
