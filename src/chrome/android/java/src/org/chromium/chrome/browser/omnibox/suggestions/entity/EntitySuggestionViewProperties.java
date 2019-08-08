// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.graphics.Bitmap;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties associated with rendering the entity suggestion view.
 */
class EntitySuggestionViewProperties {
    /** The delegate to handle actions on the suggestion view. */
    public static final WritableObjectPropertyKey<SuggestionViewDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    /** Text content for the first line of text (subject). */
    public static final WritableObjectPropertyKey<String> SUBJECT_TEXT =
            new WritableObjectPropertyKey<>();
    /** Text content for the second line of text (description). */
    public static final WritableObjectPropertyKey<String> DESCRIPTION_TEXT =
            new WritableObjectPropertyKey<>();
    /** Image to be presented beside entity details. */
    public static final WritableObjectPropertyKey<Bitmap> IMAGE_BITMAP =
            new WritableObjectPropertyKey<>();
    /** Image dominant color to be used until we have the target image. */
    public static final WritableIntPropertyKey IMAGE_DOMINANT_COLOR = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {
            DELEGATE, SUBJECT_TEXT, DESCRIPTION_TEXT, IMAGE_BITMAP, IMAGE_DOMINANT_COLOR};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
