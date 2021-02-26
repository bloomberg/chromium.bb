// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Color;

import androidx.annotation.Nullable;

import org.chromium.ui.util.ColorUtils;

/**
 * Object that contains the state of a tab, including its navigation history.
 */
public class TabState {
    /** Special value for mTimestampMillis. */
    private static final long TIMESTAMP_NOT_SET = -1;

    /** A theme color that indicates an unspecified state. */
    public static final int UNSPECIFIED_THEME_COLOR = Color.TRANSPARENT;

    /** Navigation history of the WebContents. */
    public WebContentsState contentsState;
    public int parentId = Tab.INVALID_TAB_ID;
    public int rootId;

    public long timestampMillis = TIMESTAMP_NOT_SET;
    public String openerAppId;

    /**
     * The tab's brand theme color. Set this to {@link #UNSPECIFIED_THEME_COLOR} for an unspecified
     * state.
     */
    public int themeColor = UNSPECIFIED_THEME_COLOR;

    public @Nullable @TabLaunchType Integer tabLaunchTypeAtCreation;

    /** Whether this TabState was created from a file containing info about an incognito Tab. */
    protected boolean mIsIncognito;

    public boolean isIncognito() {
        return mIsIncognito;
    }

    /** @return The theme color of the tab or {@link #UNSPECIFIED_THEME_COLOR} if not set. */
    public int getThemeColor() {
        return themeColor;
    }

    /** @return True if the tab has a theme color set. */
    public boolean hasThemeColor() {
        return themeColor != UNSPECIFIED_THEME_COLOR && ColorUtils.isValidThemeColor(themeColor);
    }
}
