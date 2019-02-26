// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests for the Android specific HTTP auth UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeHttpAuthHandlerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    public void authDialogDismissOnNavigation() throws Exception {
        ChromeHttpAuthHandler handler = triggerAuth();
        verifyAuthDialogVisibility(handler, true);
        ChromeTabUtils.loadUrlOnUiThread(mActivityTestRule.getActivity().getActivityTab(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        verifyAuthDialogVisibility(handler, false);
    }

    private ChromeHttpAuthHandler triggerAuth() throws Exception {
        AtomicReference<ChromeHttpAuthHandler> handlerRef = new AtomicReference<>();
        CallbackHelper handlerCallback = new CallbackHelper();
        Callback<ChromeHttpAuthHandler> callback = (handler) -> {
            handlerRef.set(handler);
            handlerCallback.notifyCalled();
        };
        ThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeHttpAuthHandler.setTestCreationCallback(callback); });

        String url = mTestServer.getURL("/auth-basic");
        ChromeTabUtils.loadUrlOnUiThread(mActivityTestRule.getActivity().getActivityTab(), url);
        handlerCallback.waitForCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeHttpAuthHandler.setTestCreationCallback(null); });
        return handlerRef.get();
    }

    private void verifyAuthDialogVisibility(ChromeHttpAuthHandler handler, boolean isVisible) {
        CriteriaHelper.pollUiThread(
                Criteria.equals(isVisible, () -> handler.isShowingAuthDialog()));
    }
}
