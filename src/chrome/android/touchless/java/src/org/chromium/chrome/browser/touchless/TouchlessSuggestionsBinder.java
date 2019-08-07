// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.browser.suggestions.SuggestionsBinder;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.touchless.R;

/** Overrides SuggestionsBinder methods to provide touchless specific values and functionality. */
public class TouchlessSuggestionsBinder extends SuggestionsBinder {
    private static final String TOUCHLESS_ARTICLE_AGE_FORMAT_STRING = " %s";

    public TouchlessSuggestionsBinder(View cardContainerView, SuggestionsUiDelegate uiDelegate) {
        super(cardContainerView, uiDelegate);
    }

    @Override
    protected int getThumbnailSize() {
        return mCardContainerView.getResources().getDimensionPixelSize(
                R.dimen.touchless_snippets_thumbnail_size);
    }

    @Override
    protected int getPublisherFaviconSizePx() {
        return getPublisherIconTextView().getResources().getDimensionPixelSize(
                R.dimen.touchless_publisher_icon_size);
    }

    @Override
    protected String getArticleAgeFormatString() {
        return TOUCHLESS_ARTICLE_AGE_FORMAT_STRING;
    }

    @Override
    protected TextView getPublisherIconTextView() {
        return mAgeTextView;
    }

    @Override
    protected Drawable createThumbnailDrawable(Bitmap thumbnail) {
        return ViewUtils.createRoundedBitmapDrawable(thumbnail,
                mAgeTextView.getResources().getDimensionPixelOffset(
                        R.dimen.default_rounded_corner_radius));
    }
}