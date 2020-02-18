// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.ui.system.StatusBarColorController.UNDEFINED_STATUS_BAR_COLOR;

import android.content.res.Resources;

import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.chrome.browser.util.ColorUtils;

import javax.inject.Inject;

/**
 * Manages the status bar color for a CustomTabActivity.
 */
@ActivityScope
public class CustomTabStatusBarColorProvider {
    private final Resources mResources;
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final CustomTabActivityTabProvider mCustomTabActivityTabProvider;
    private final StatusBarColorController mStatusBarColorController;
    private final TabThemeColorHelperWrapper mTabThemeColorHelperWrapper;

    private boolean mUseTabThemeColor;
    private boolean mCachedIsFallbackColorDefault;

    /** Wrapper class to help unit testing. */
    static class TabThemeColorHelperWrapper {
        boolean isDefaultColorUsed(Tab tab) {
            return TabThemeColorHelper.isDefaultColorUsed(tab);
        }
    }

    /** Constructor for production use. */
    @Inject
    public CustomTabStatusBarColorProvider(Resources resources,
            CustomTabIntentDataProvider intentDataProvider,
            CustomTabActivityTabProvider customTabActivityTabProvider,
            StatusBarColorController statusBarColorController) {
        this(resources, intentDataProvider, customTabActivityTabProvider, statusBarColorController,
                new TabThemeColorHelperWrapper());
    }

    /** Constructor for unit testing that allows using a custom TabThemeColorHelperWrapper. */
    public CustomTabStatusBarColorProvider(Resources resources,
            CustomTabIntentDataProvider intentDataProvider,
            CustomTabActivityTabProvider customTabActivityTabProvider,
            StatusBarColorController statusBarColorController,
            TabThemeColorHelperWrapper tabThemeColorHelperWrapper) {
        mResources = resources;
        mIntentDataProvider = intentDataProvider;
        mCustomTabActivityTabProvider = customTabActivityTabProvider;
        mStatusBarColorController = statusBarColorController;
        mTabThemeColorHelperWrapper = tabThemeColorHelperWrapper;
    }

    /**
     * Sets whether the tab's theme color should be used for the status bar and triggers an update
     * of the status bar color if needed.
     */
    public void setUseTabThemeColor(boolean useTabThemeColor) {
        if (mUseTabThemeColor == useTabThemeColor) return;

        mUseTabThemeColor = useTabThemeColor;

        // We keep the last value that {@link #isStatusBarDefaultThemeColor} was called with so we
        // can use it here. Getting the live value would be somewhat complicated - we'd need to call
        // isStatusBarDefaultThemeColor on the ChromeActivity, not the CustomTabActivity. At the
        // time of writing I don't believe this is worth the complexity as:
        // - ChromeActivity#isStatusBarDefaultThemeColor always returns false.
        // - Our isStatusBarDefaultThemeColor method only uses the fallback color when the custom
        //   tab is opened by Chrome, and we don't call setUseTabThemeColor in this case.
        mStatusBarColorController.updateStatusBarColor(
                isStatusBarDefaultThemeColor(mCachedIsFallbackColorDefault));
    }

    int getBaseStatusBarColor(int fallbackStatusBarColor) {
        if (mIntentDataProvider.isOpenedByChrome()) return fallbackStatusBarColor;

        Tab tab = mCustomTabActivityTabProvider.getTab();
        if (tab != null) {
            if (tab.isPreview()) return ColorUtils.getDefaultThemeColor(mResources, false);

            // Returning the color as undefined causes the Tab's theme color to be used.
            if (mUseTabThemeColor && !mTabThemeColorHelperWrapper.isDefaultColorUsed(tab)) {
                return UNDEFINED_STATUS_BAR_COLOR;
            }
        }

        return mIntentDataProvider.getToolbarColor();
    }

    boolean isStatusBarDefaultThemeColor(boolean isFallbackColorDefault) {
        mCachedIsFallbackColorDefault = isFallbackColorDefault;
        if (mIntentDataProvider.isOpenedByChrome()) return isFallbackColorDefault;

        if (mUseTabThemeColor) {
            Tab tab = mCustomTabActivityTabProvider.getTab();
            if (tab != null) return mTabThemeColorHelperWrapper.isDefaultColorUsed(tab);
        }

        return false;
    }

    /**
     * Called when toolbar color is changed so that the status bar can adapt.
     */
    public void onToolbarColorChanged() {
        mStatusBarColorController.updateStatusBarColor(false);
    }
}
