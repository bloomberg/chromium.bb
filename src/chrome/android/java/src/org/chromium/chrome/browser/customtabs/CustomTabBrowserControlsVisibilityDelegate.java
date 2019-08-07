// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabBrowserControlsState;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Implementation of {@link BrowserControlsVisibilityDelegate} for custom tabs.
 */
@ActivityScope
public class CustomTabBrowserControlsVisibilityDelegate
        implements BrowserControlsVisibilityDelegate {
    private final Lazy<ChromeFullscreenManager> mFullscreenManagerDelegate;
    private final ActivityTabProvider mTabProvider;
    private boolean mIsInTwaMode;
    private boolean mIsInModuleLoadingMode;

    @Inject
    public CustomTabBrowserControlsVisibilityDelegate(
            Lazy<ChromeFullscreenManager> fullscreenManager, ActivityTabProvider tabProvider) {
        mFullscreenManagerDelegate = fullscreenManager;
        mTabProvider = tabProvider;
    }

    /**
     * Sets trusted web activity mode. In trusted web activity mode browser controls should be
     * hidden.
     */
    public void setTrustedWebActivityMode(boolean isInTwaMode) {
        if (mIsInTwaMode == isInTwaMode) {
            return;
        }
        mIsInTwaMode = isInTwaMode;
        updateActiveTabFullscreenEnabledState();
    }

    /**
     * Sets module loading mode. In module loading mode browser controls should be hidden.
     */
    public void setModuleLoadingMode(boolean isInModuleLoadingMode) {
        if (mIsInModuleLoadingMode == isInModuleLoadingMode) {
            return;
        }
        mIsInModuleLoadingMode = isInModuleLoadingMode;
        updateActiveTabFullscreenEnabledState();
    }

    @Override
    public boolean canShowBrowserControls() {
        return !mIsInTwaMode && !mIsInModuleLoadingMode
                && getDefaultVisibilityDelegate().canShowBrowserControls();
    }

    @Override
    public boolean canAutoHideBrowserControls() {
        return getDefaultVisibilityDelegate().canAutoHideBrowserControls();
    }

    private BrowserStateBrowserControlsVisibilityDelegate getDefaultVisibilityDelegate() {
        return mFullscreenManagerDelegate.get().getBrowserVisibilityDelegate();
    }

    private void updateActiveTabFullscreenEnabledState() {
        TabBrowserControlsState.updateEnabledState(mTabProvider.get());
    }
}
