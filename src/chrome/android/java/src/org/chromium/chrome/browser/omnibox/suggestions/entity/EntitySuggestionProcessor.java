// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** A class that handles model and view creation for the Entity suggestions. */
public class EntitySuggestionProcessor extends BaseSuggestionViewProcessor {
    private static final String TAG = "EntitySP";
    private final Context mContext;
    private final SuggestionHost mSuggestionHost;
    private final Map<GURL, List<PropertyModel>> mPendingImageRequests;
    private final int mEntityImageSizePx;
    private final Supplier<ImageFetcher> mImageFetcherSupplier;
    // Threshold for low RAM devices. We won't be showing entity suggestion images
    // on devices that have less RAM than this to avoid bloat and reduce user-visible
    // slowdown while spinning up an image decompression process.
    // We set the threshold to 1.5GB to reduce number of users affected by this restriction.
    // TODO(crbug.com/979426): This threshold is set arbitrarily and should be revisited once
    // we have more data about relation between system memory and performance.
    private static final int LOW_MEMORY_THRESHOLD_KB =
            (int) (1.5 * ConversionUtils.KILOBYTES_PER_GIGABYTE);

    // These values are used with UMA to report Omnibox.RichEntity.DecorationType histograms, and
    // should therefore be treated as append-only.
    // See http://cs.chromium.org/Omnibox.RichEntity.DecorationType.
    private static final int DECORATION_TYPE_ICON = 0;
    private static final int DECORATION_TYPE_COLOR = 1;
    private static final int DECORATION_TYPE_IMAGE = 2;
    private static final int DECORATION_TYPE_TOTAL_COUNT = 3;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     */
    public EntitySuggestionProcessor(Context context, SuggestionHost suggestionHost,
            Supplier<ImageFetcher> imageFetcherSupplier) {
        super(context, suggestionHost);
        mContext = context;
        mSuggestionHost = suggestionHost;
        mPendingImageRequests = new HashMap<>();
        mEntityImageSizePx = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_entity_icon_size);
        mImageFetcherSupplier = imageFetcherSupplier;
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion) {
        return suggestion.getType() == OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.ENTITY_SUGGESTION;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(EntitySuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void recordItemPresented(PropertyModel model) {
        // SuggestionUsed bookkeeping handled in C++:
        // http://cs.chromium.org/Omnibox.SuggestionUsed.RichEntity
        int decorationType = model.get(EntitySuggestionViewProperties.DECORATION_TYPE);
        RecordHistogram.recordEnumeratedHistogram(
                "Omnibox.RichEntity.DecorationType", decorationType, DECORATION_TYPE_TOTAL_COUNT);
    }

    private void fetchEntityImage(OmniboxSuggestion suggestion, PropertyModel model) {
        ThreadUtils.assertOnUiThread();
        final GURL url = suggestion.getImageUrl();
        if (url.isEmpty()) return;

        // Ensure an image fetcher is available prior to requesting images.
        ImageFetcher imageFetcher = mImageFetcherSupplier.get();
        if (imageFetcher == null) return;

        // Do not make duplicate answer image requests for the same URL (to avoid generating
        // duplicate bitmaps for the same image).
        if (mPendingImageRequests.containsKey(url)) {
            mPendingImageRequests.get(url).add(model);
            return;
        }

        List<PropertyModel> models = new ArrayList<>();
        models.add(model);
        mPendingImageRequests.put(url, models);

        imageFetcher.fetchImage(url.getSpec(), ImageFetcher.ENTITY_SUGGESTIONS_UMA_CLIENT_NAME,
                mEntityImageSizePx, mEntityImageSizePx, (Bitmap bitmap) -> {
                    ThreadUtils.assertOnUiThread();

                    final List<PropertyModel> pendingModels = mPendingImageRequests.remove(url);
                    if (pendingModels == null || bitmap == null) {
                        return;
                    }

                    for (int i = 0; i < pendingModels.size(); i++) {
                        PropertyModel pendingModel = pendingModels.get(i);
                        setSuggestionDrawableState(pendingModel,
                                SuggestionDrawableState.Builder.forBitmap(mContext, bitmap)
                                        .setUseRoundedCorners(true)
                                        .setLarge(true)
                                        .build());
                        pendingModel.set(EntitySuggestionViewProperties.DECORATION_TYPE,
                                DECORATION_TYPE_IMAGE);
                    }
                });
    }

    @VisibleForTesting
    public void applyImageDominantColor(String colorSpec, PropertyModel model) {
        if (TextUtils.isEmpty(colorSpec)) {
            return;
        }

        int color;
        try {
            color = Color.parseColor(colorSpec);
        } catch (IllegalArgumentException e) {
            Log.i(TAG, "Failed to parse dominant color: " + colorSpec);
            return;
        }

        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder.forColor(color)
                        .setLarge(true)
                        .setUseRoundedCorners(true)
                        .build());
        model.set(EntitySuggestionViewProperties.DECORATION_TYPE, DECORATION_TYPE_COLOR);
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder
                        .forDrawableRes(mContext, R.drawable.ic_suggestion_magnifier)
                        .setAllowTint(true)
                        .build());
        model.set(EntitySuggestionViewProperties.DECORATION_TYPE, DECORATION_TYPE_ICON);

        if (SysUtils.amountOfPhysicalMemoryKB() >= LOW_MEMORY_THRESHOLD_KB
                || CommandLine.getInstance().hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)) {
            applyImageDominantColor(suggestion.getImageDominantColor(), model);
            fetchEntityImage(suggestion, model);
        }

        model.set(EntitySuggestionViewProperties.SUBJECT_TEXT, suggestion.getDisplayText());
        model.set(EntitySuggestionViewProperties.DESCRIPTION_TEXT, suggestion.getDescription());
        setRefineAction(model, suggestion);
    }
}
