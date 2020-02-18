// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;

import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * List of properties to designate information about a single tab.
 */
public class TabProperties {
    public static final PropertyModel.WritableIntPropertyKey TAB_ID =
            new PropertyModel.WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<TabListMediator.TabActionListener>
            TAB_SELECTED_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<TabListMediator.TabActionListener>
            TAB_CLOSED_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Drawable> FAVICON =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<TabListMediator.ThumbnailFetcher>
            THUMBNAIL_FETCHER = new WritableObjectPropertyKey<>(true);

    public static final WritableObjectPropertyKey<TabListMediator.IphProvider> IPH_PROVIDER =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<TabListMediator.TabActionListener>
            CREATE_GROUP_LISTENER = new WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableFloatPropertyKey ALPHA =
            new PropertyModel.WritableFloatPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey CARD_ANIMATION_STATUS =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<TabListMediator.TabActionListener>
            SELECTABLE_TAB_CLICKED_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<SelectionDelegate> TAB_SELECTION_DELEGATE =
            new WritableObjectPropertyKey<>();

    public static final PropertyModel.ReadableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.ReadableBooleanPropertyKey();

    public static final PropertyModel.ReadableIntPropertyKey SELECTED_TAB_BACKGROUND_DRAWABLE_ID =
            new PropertyModel.ReadableIntPropertyKey();

    public static final PropertyModel.ReadableIntPropertyKey TABSTRIP_FAVICON_BACKGROUND_COLOR_ID =
            new PropertyModel.ReadableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS_TAB_GRID = new PropertyKey[] {TAB_ID,
            TAB_SELECTED_LISTENER, TAB_CLOSED_LISTENER, FAVICON, THUMBNAIL_FETCHER, IPH_PROVIDER,
            TITLE, IS_SELECTED, CREATE_GROUP_LISTENER, ALPHA, CARD_ANIMATION_STATUS,
            SELECTABLE_TAB_CLICKED_LISTENER, TAB_SELECTION_DELEGATE, IS_INCOGNITO,
            SELECTED_TAB_BACKGROUND_DRAWABLE_ID, TABSTRIP_FAVICON_BACKGROUND_COLOR_ID};

    public static final PropertyKey[] ALL_KEYS_TAB_STRIP =
            new PropertyKey[] {TAB_ID, TAB_SELECTED_LISTENER, TAB_CLOSED_LISTENER, FAVICON,
                    IS_SELECTED, TITLE, TABSTRIP_FAVICON_BACKGROUND_COLOR_ID};
}
