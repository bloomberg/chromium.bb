// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.ClientAppDataRegister;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabNightModeStateController;

import dagger.Module;
import dagger.Provides;

/**
 * Module for custom tab specific bindings.
 */
@Module
public class CustomTabActivityModule {
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final CustomTabNightModeStateController mNightModeController;

    public CustomTabActivityModule(CustomTabIntentDataProvider intentDataProvider,
            CustomTabNightModeStateController nightModeController) {
        mIntentDataProvider = intentDataProvider;
        mNightModeController = nightModeController;
    }

    @Provides
    public CustomTabIntentDataProvider provideIntentDataProvider() {
        return mIntentDataProvider;
    }

    @Provides
    public ClientAppDataRegister provideClientAppDataRegister() {
        return new ClientAppDataRegister();
    }

    @Provides
    public CustomTabNightModeStateController provideNightModeController() {
        return mNightModeController;
    }
}
