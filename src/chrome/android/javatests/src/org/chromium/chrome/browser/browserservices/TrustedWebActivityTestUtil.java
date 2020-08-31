// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.TrustedWebUtils;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.BrowserControlsState;

import java.util.concurrent.TimeoutException;

/**
 * Common utilities for Trusted Web Activity tests.
 */
public class TrustedWebActivityTestUtil {
    /**
     * Waits till verification either succeeds or fails.
     */
    private static class CurrentPageVerifierWaiter extends CallbackHelper {
        private CurrentPageVerifier mVerifier;

        public void start(CurrentPageVerifier verifier) throws TimeoutException {
            mVerifier = verifier;
            if (checkShouldNotify()) return;

            mVerifier.addVerificationObserver(this::onVerificationUpdate);
            waitForFirst();
        }

        public void onVerificationUpdate() {
            if (checkShouldNotify()) {
                mVerifier.removeVerificationObserver(this::onVerificationUpdate);
            }
        }

        public boolean checkShouldNotify() {
            return mVerifier.getState().status != CurrentPageVerifier.VerificationStatus.PENDING;
        }
    }

    /** Creates an Intent that will launch a Custom Tab to the given |url|. */
    public static Intent createTrustedWebActivityIntent(String url) {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), url);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
        return intent;
    }

    /** Caches a successful verification for the given |packageName| and |url|. */
    public static void spoofVerification(String packageName, String url) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> OriginVerifier.addVerificationOverride(packageName, Origin.create(url),
                        CustomTabsService.RELATION_HANDLE_ALL_URLS));
    }

    /** Creates a Custom Tabs Session from the Intent, specifying the |packageName|. */
    public static void createSession(Intent intent, String packageName) throws TimeoutException {
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, packageName);
    }

    /** Checks if given instance of {@link CustomTabActivity} is a Trusted Web Activity. */
    public static boolean isTrustedWebActivity(CustomTabActivity activity) {
        // A key part of the Trusted Web Activity UI is the lack of browser controls.
        @BrowserControlsState
        int constraints = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return TabBrowserControlsConstraintsHelper.getConstraints(activity.getActivityTab());
        });
        return constraints == BrowserControlsState.HIDDEN;
    }

    /**
     * Waits till {@link CurrentPageVerifier} verification either succeeds or fails.
     */
    public static void waitForCurrentPageVerifierToFinish(CustomTabActivity activity)
            throws TimeoutException {
        CurrentPageVerifier verifier = activity.getComponent().resolveCurrentPageVerifier();
        new CurrentPageVerifierWaiter().start(verifier);
    }
}
