// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * Represents the data for a header of a group of snippets.
 */
public class SectionHeaderProperties {
    /** The header text to be shown. */
    public static final PropertyModel.WritableObjectPropertyKey<String> HEADER_TEXT_KEY =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey UNREAD_CONTENT_KEY =
            new PropertyModel.WritableBooleanPropertyKey();
    // This is for setting the visibility of the options dropdown indicator.
    public static final PropertyModel
            .WritableObjectPropertyKey<ViewVisibility> OPTIONS_INDICATOR_VISIBILITY_KEY =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static PropertyModel createSectionHeader(String headerText) {
        return new PropertyModel
                .Builder(HEADER_TEXT_KEY, UNREAD_CONTENT_KEY, OPTIONS_INDICATOR_VISIBILITY_KEY)
                .with(HEADER_TEXT_KEY, headerText)
                .with(UNREAD_CONTENT_KEY, false)
                .build();
    }
}
