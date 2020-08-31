// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.CurrentPageVerifier.VerificationState;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationHandle;

/**
 * Handles recording user metrics for active tab.
 */
public class WebappActiveTabUmaTracker extends ActivityTabTabObserver {
    @VisibleForTesting
    public static final String HISTOGRAM_NAVIGATION_STATUS = "Webapp.NavigationStatus";

    private BrowserServicesIntentDataProvider mIntentDataProvider;
    private CurrentPageVerifier mCurrentPageVerifier;

    public WebappActiveTabUmaTracker(ActivityTabProvider tabProvider,
            BrowserServicesIntentDataProvider intentDataProvider,
            CurrentPageVerifier currentPageVerifier) {
        super(tabProvider);
        mIntentDataProvider = intentDataProvider;
        mCurrentPageVerifier = currentPageVerifier;
    }

    @Override
    public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
        if (navigation.hasCommitted() && navigation.isInMainFrame()
                && !navigation.isSameDocument()) {
            RecordHistogram.recordBooleanHistogram(
                    HISTOGRAM_NAVIGATION_STATUS, !navigation.isErrorPage());

            if (mIntentDataProvider.isWebApkActivity() && tab.getParentId() == Tab.INVALID_TAB_ID) {
                VerificationState verificationState = mCurrentPageVerifier.getState();
                boolean isNavigationInScope = (verificationState == null
                        || verificationState.status != VerificationStatus.FAILURE);
                WebApkUma.recordNavigation(isNavigationInScope);
            }
        }
    }
}
