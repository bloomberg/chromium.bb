// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * The share button.
 */
class ShareButton extends ChromeImageButton implements TintObserver {
    /** A provider that notifies components when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

    /** The {@link sActivityTabTabObserver} used to know when the active page changed. */
    private ActivityTabTabObserver mActivityTabTabObserver;

    public ShareButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addTintObserver(this);
    }

    void setActivityTabProvider(ActivityTabProvider activityTabProvider) {
        mActivityTabTabObserver = new ActivityTabTabObserver(activityTabProvider) {
            @Override
            public void onObservingDifferentTab(Tab tab) {
                if (tab == null) return;
                setEnabled(shouldEnableShare(tab));
            }

            @Override
            public void onUpdateUrl(Tab tab, String url) {
                if (tab == null) return;
                setEnabled(shouldEnableShare(tab));
            }
        };
    }

    void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeTintObserver(this);
            mThemeColorProvider = null;
        }
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            mActivityTabTabObserver = null;
        }
    }

    private static boolean shouldEnableShare(Tab tab) {
        final String url = tab.getUrl();
        final boolean isChromeScheme = url.startsWith(UrlConstants.CHROME_URL_PREFIX)
                || url.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX);
        return !isChromeScheme && !tab.isShowingInterstitialPage();
    }

    @Override
    public void onTintChanged(ColorStateList tint, boolean useLight) {
        ApiCompatibilityUtils.setImageTintList(this, tint);
    }
}
