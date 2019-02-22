// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.chrome.browser.vr.util.PermissionUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;

/**
 * WebXR for AR-specific implementation of the WebXrTestFramework.
 */
public class WebXrArTestFramework extends WebXrTestFramework {
    /**
     * Must be constructed after the rule has been applied (e.g. in whatever method is
     * tagged with @Before).
     */
    public WebXrArTestFramework(ChromeActivityTestRule rule) {
        super(rule);
    }

    /**
     * Requests an AR session, automatically accepting the Camera permission prompt if necessary.
     * Causes a test failure if it is unable to do so.
     *
     * @param webContents The Webcontents to start the AR session in.
     */
    @Override
    public void enterSessionWithUserGestureOrFail(WebContents webContents) {
        runJavaScriptOrFail(
                "sessionTypeToRequest = sessionTypes.AR", POLL_TIMEOUT_LONG_MS, webContents);
        // Requesting an AR session for the first time on a page will always prompt for camera
        // permissions, but not on subsequent requests, so check to see if we'll need to accept it
        // after requesting the session.
        boolean expectPermissionPrompt = permissionRequestWouldTriggerPrompt("camera", webContents);
        // TODO(bsheedy): Rename enterPresentation since it's used for both presentation and AR?
        enterSessionWithUserGesture(webContents);
        if (expectPermissionPrompt) {
            PermissionUtils.waitForPermissionPrompt();
            PermissionUtils.acceptPermissionPrompt();
        }
        pollJavaScriptBooleanOrFail("sessionInfos[sessionTypes.AR].currentSession != null",
                POLL_TIMEOUT_LONG_MS, webContents);
    }

    /**
     * Exits a WebXR AR session.
     *
     * @param webcontents The WebContents to exit the AR session in
     */
    @Override
    public void endSession(WebContents webContents) {
        runJavaScriptOrFail("sessionInfos[sessionTypes.AR].currentSession.end()",
                POLL_TIMEOUT_SHORT_MS, webContents);
    }
}