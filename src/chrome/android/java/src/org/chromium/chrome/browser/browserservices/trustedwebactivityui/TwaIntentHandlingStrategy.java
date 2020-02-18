// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.sharing.TwaSharingController;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.content.DefaultCustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;

import javax.inject.Inject;

/**
 * TWA-specific implementation of {@link CustomTabIntentHandlingStrategy}.
 * Currently adds Web Share Target capabilities on top of the Custom Tabs intent handling.
 */
@ActivityScope
public class TwaIntentHandlingStrategy implements CustomTabIntentHandlingStrategy {
    private final DefaultCustomTabIntentHandlingStrategy mDefaultStrategy;
    private final TwaSharingController mSharingController;

    @Inject
    public TwaIntentHandlingStrategy(DefaultCustomTabIntentHandlingStrategy defaultStrategy,
            TwaSharingController sharingController) {
        mDefaultStrategy = defaultStrategy;
        mSharingController = sharingController;
    }

    @Override
    public void handleInitialIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        handleIntent(intentDataProvider);
    }

    @Override
    public void handleNewIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        // TODO(pshmakov): we can have a significant delay here in case of POST sharing.
        // Allow showing splash screen, if it's provided in the intent.
        handleIntent(intentDataProvider);
    }

    private void handleIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        mSharingController.deliverToShareTarget(intentDataProvider).then((delivered) -> {
            if (!delivered) {
                mDefaultStrategy.handleInitialIntent(intentDataProvider);
            }
        });
    }
}
