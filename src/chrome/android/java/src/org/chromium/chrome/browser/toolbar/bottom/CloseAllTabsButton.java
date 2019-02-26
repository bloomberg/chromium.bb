// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.annotation.StringRes;
import android.util.AttributeSet;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.TabCountProvider.TabCountObserver;
import org.chromium.chrome.browser.toolbar.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ThemeColorProvider.ThemeColorObserver;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * The close all tabs button.
 */
class CloseAllTabsButton extends ChromeImageButton
        implements ThemeColorObserver, IncognitoStateObserver, TabCountObserver {
    /** A provider that notifies when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

    /** A provider that notifies when incognito mode is entered or exited. */
    private IncognitoStateProvider mIncognitoStateProvider;

    /** A provider that notifies when the number of tabs changes. */
    private TabCountProvider mTabCountProvider;

    /** Whether the close all tabs button should be enabled. */
    private boolean mIsEnabled = true;

    public CloseAllTabsButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeObserver(this);
            mThemeColorProvider = null;
        }
        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver((IncognitoStateObserver) this);
            mIncognitoStateProvider = null;
        }
        if (mTabCountProvider != null) {
            mTabCountProvider.removeObserver(this);
            mTabCountProvider = null;
        }
    }

    void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addObserver(this);
    }

    @Override
    public void onThemeColorChanged(ColorStateList tint, int primaryColor) {
        ApiCompatibilityUtils.setImageTintList(this, tint);
    }

    void setIncognitoStateProvider(IncognitoStateProvider incognitoStateProvider) {
        mIncognitoStateProvider = incognitoStateProvider;
        mIncognitoStateProvider.addObserver((IncognitoStateObserver) this);
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        @StringRes
        int resId;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_STRINGS)) {
            resId = isIncognito ? R.string.menu_close_all_private_tabs
                                : R.string.menu_close_all_tabs;
        } else {
            resId = isIncognito ? R.string.menu_close_all_incognito_tabs
                                : R.string.menu_close_all_tabs;
        }
        setContentDescription(getResources().getText(resId));
    }

    void setTabCountProvider(TabCountProvider provider) {
        mTabCountProvider = provider;
        mTabCountProvider.addObserver(this);
    }

    @Override
    public void onTabCountChanged(int tabCount, boolean isIncognito) {
        final boolean shouldBeEnabled = tabCount > 0;
        if (shouldBeEnabled == mIsEnabled) return;

        mIsEnabled = shouldBeEnabled;
        setEnabled(mIsEnabled);
    }
}
