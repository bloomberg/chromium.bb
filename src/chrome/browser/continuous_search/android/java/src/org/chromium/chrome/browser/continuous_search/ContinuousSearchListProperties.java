// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains model properties for a single search list item in Continuous Search Navigation.
 */
class ContinuousSearchListProperties {
    @IntDef({ListItemType.GROUP_LABEL, ListItemType.SEARCH_RESULT, ListItemType.AD})
    @Retention(RetentionPolicy.SOURCE)

    public @interface ListItemType {
        int GROUP_LABEL = 0;
        int SEARCH_RESULT = 1;
        int AD = 2;
    }

    public static final WritableObjectPropertyKey<String> LABEL = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<GURL> URL = new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey BORDER_COLOR = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey BACKGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey TITLE_TEXT_STYLE = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey DESCRIPTION_TEXT_STYLE =
            new WritableIntPropertyKey();
    public static final WritableIntPropertyKey FOREGROUND_COLOR = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<OnClickListener> DISMISS_CLICK_CALLBACK =
            new WritableObjectPropertyKey();

    /**
     * Properties used for individual items shown in the RecyclerView.
     */
    public static final PropertyKey[] ITEM_KEYS = {LABEL, URL, IS_SELECTED, BORDER_COLOR,
            CLICK_LISTENER, BACKGROUND_COLOR, TITLE_TEXT_STYLE, DESCRIPTION_TEXT_STYLE};
    /**
     * Properties used for the root view. The root view currently contains the RecyclerView
     * and the dismiss button.
     */
    public static final PropertyKey[] ROOT_VIEW_KEYS = {
            BACKGROUND_COLOR, FOREGROUND_COLOR, DISMISS_CLICK_CALLBACK};
}
