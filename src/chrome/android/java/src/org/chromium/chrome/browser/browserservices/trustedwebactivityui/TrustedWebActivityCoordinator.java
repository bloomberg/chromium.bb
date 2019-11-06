// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityDisclosureController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityOpenTimeRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityToolbarController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityVerifier;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.splashscreen.TwaSplashController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.TrustedWebActivityDisclosureView;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.TrustedWebActivityToolbarView;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Coordinator for the Trusted Web Activity component.
 * Add methods here if other components need to communicate with Trusted Web Activity component.
 */
@ActivityScope
public class TrustedWebActivityCoordinator {
    @Inject
    public TrustedWebActivityCoordinator(
            TrustedWebActivityDisclosureController disclosureController,
            TrustedWebActivityToolbarController toolbarController,
            TrustedWebActivityToolbarView toolbarView,
            TrustedWebActivityDisclosureView disclosureView,
            TrustedWebActivityOpenTimeRecorder openTimeRecorder,
            TrustedWebActivityVerifier verifier,
            CustomTabActivityNavigationController navigationController,
            Lazy<TwaSplashController> splashController,
            CustomTabIntentDataProvider intentDataProvider,
            TrustedWebActivityUmaRecorder umaRecorder,
            CustomTabStatusBarColorProvider statusBarColorProvider) {
        // We don't need to do anything with most of the classes above, we just need to resolve them
        // so they start working.

        navigationController.setLandingPageOnCloseCriterion(verifier::isPageOnVerifiedOrigin);

        initSplashScreen(splashController, intentDataProvider, umaRecorder);
        useTabThemeColorWhenVerified(statusBarColorProvider, verifier);
    }

    private void initSplashScreen(Lazy<TwaSplashController> splashController,
            CustomTabIntentDataProvider intentDataProvider,
            TrustedWebActivityUmaRecorder umaRecorder) {
        boolean showSplashScreen =
                TwaSplashController.intentIsForTwaWithSplashScreen(intentDataProvider.getIntent());

        if (showSplashScreen) {
            splashController.get();
        }

        umaRecorder.recordSplashScreenUsage(showSplashScreen);
    }

    private void useTabThemeColorWhenVerified(CustomTabStatusBarColorProvider colorProvider,
            TrustedWebActivityVerifier verifier) {
        verifier.addVerificationObserver(() -> {
            if (verifier.getState() == null) return;

            @VerificationStatus int status = verifier.getState().status;
            colorProvider.setUseTabThemeColor(
                    status == VerificationStatus.PENDING || status == VerificationStatus.SUCCESS);
        });
    }
}
