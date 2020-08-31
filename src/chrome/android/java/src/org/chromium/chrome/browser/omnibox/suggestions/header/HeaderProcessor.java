// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.DropdownItemProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.ui.modelutil.PropertyModel;

/** A class that handles model and view creation for the suggestion headers. */
public class HeaderProcessor implements DropdownItemProcessor {
    private final SuggestionHost mSuggestionHost;
    private final UrlBarDelegate mUrlBarDelegate;
    private final int mMinimumHeight;

    /**
     * @param context An Android context.
     */
    public HeaderProcessor(
            Context context, SuggestionHost suggestionHost, UrlBarDelegate urlDelegate) {
        mSuggestionHost = suggestionHost;
        mUrlBarDelegate = urlDelegate;
        mMinimumHeight = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_header_height);
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.HEADER;
    }

    @Override
    public int getMinimumViewHeight() {
        return mMinimumHeight;
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {}

    @Override
    public void onNativeInitialized() {}

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(HeaderViewProperties.ALL_KEYS);
    }

    @Override
    public void recordItemPresented(PropertyModel model) {}

    @Override
    public void onSuggestionsReceived() {}

    /**
     * Populate a model for the group header.
     * @param model The model to populate.
     * @param headerText Text to be displayed for this group header.
     */
    public void populateModel(
            final PropertyModel model, final int groupId, final String headerText) {
        model.set(HeaderViewProperties.TITLE, headerText);
        model.set(HeaderViewProperties.IS_EXPANDED, true);
        model.set(HeaderViewProperties.DELEGATE, new HeaderViewProperties.Delegate() {
            @Override
            public void onHeaderSelected() {
                mUrlBarDelegate.setOmniboxEditingText(null);
            }

            @Override
            public void onHeaderClicked() {
                boolean newState = !model.get(HeaderViewProperties.IS_EXPANDED);
                model.set(HeaderViewProperties.IS_EXPANDED, newState);
                mSuggestionHost.setGroupVisibility(groupId, newState);
            }
        });
    }
}
