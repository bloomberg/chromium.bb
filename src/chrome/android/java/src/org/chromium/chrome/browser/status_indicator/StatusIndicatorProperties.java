// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.graphics.drawable.Drawable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class StatusIndicatorProperties {
    /** The text that describes status. */
    static final PropertyModel.WritableObjectPropertyKey<String> STATUS_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** The {@link Drawable} that will be displayed next to the status text. */
    static final PropertyModel.WritableObjectPropertyKey<Drawable> STATUS_ICON =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** Visibility of the status indicator's Android view. */
    static final PropertyModel.WritableIntPropertyKey ANDROID_VIEW_VISIBILITY =
            new PropertyModel.WritableIntPropertyKey();

    /** Whether the composited version of the status indicator is visible. */
    static final PropertyModel.WritableBooleanPropertyKey COMPOSITED_VIEW_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    static final PropertyModel.WritableIntPropertyKey BACKGROUND_COLOR =
            new PropertyModel.WritableIntPropertyKey();

    static final PropertyModel.WritableFloatPropertyKey TEXT_ALPHA =
            new PropertyModel.WritableFloatPropertyKey();

    static final PropertyModel.WritableIntPropertyKey TEXT_COLOR =
            new PropertyModel.WritableIntPropertyKey();

    static final PropertyModel.WritableIntPropertyKey ICON_TINT =
            new PropertyModel.WritableIntPropertyKey();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {STATUS_TEXT, STATUS_ICON, ANDROID_VIEW_VISIBILITY,
                    COMPOSITED_VIEW_VISIBLE, BACKGROUND_COLOR, TEXT_ALPHA, TEXT_COLOR, ICON_TINT};
}
