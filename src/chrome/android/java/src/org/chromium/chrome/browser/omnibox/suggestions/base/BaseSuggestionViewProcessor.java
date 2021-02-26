// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Typeface;
import android.text.Spannable;
import android.text.style.StyleSpan;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion.MatchClassification;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/**
 * A class that handles base properties and model for most suggestions.
 */
public abstract class BaseSuggestionViewProcessor implements SuggestionProcessor {
    private static final String SUGGESTION_DENSITY_PARAM = "omnibox_compact_suggestions_variant";
    private static final String SUGGESTION_DENSITY_SEMICOMPACT = "semi-compact";
    private final Context mContext;
    private final SuggestionHost mSuggestionHost;
    private final int mDesiredFaviconWidthPx;
    private final int mDecorationImageSizePx;
    private int mSuggestionSizePx;
    private @BaseSuggestionViewProperties.Density int mDensity =
            BaseSuggestionViewProperties.Density.COMFORTABLE;

    /**
     * @param context Current context.
     * @param host A handle to the object using the suggestions.
     */
    public BaseSuggestionViewProcessor(Context context, SuggestionHost host) {
        mContext = context;
        mSuggestionHost = host;
        mDesiredFaviconWidthPx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);
        mDecorationImageSizePx = context.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_decoration_image_size);
        mSuggestionSizePx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_comfortable_height);
    }

    /**
     * @return The desired size of Omnibox suggestion favicon.
     */
    protected int getDesiredFaviconSize() {
        return mDesiredFaviconWidthPx;
    }

    /**
     * @return The size of suggestion decoration images in pixels.
     */
    protected int getDecorationImageSize() {
        return mDecorationImageSizePx;
    }

    @Override
    public void onNativeInitialized() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_COMPACT_SUGGESTIONS)) {
            if (SUGGESTION_DENSITY_SEMICOMPACT.equals(ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.OMNIBOX_COMPACT_SUGGESTIONS, SUGGESTION_DENSITY_PARAM))) {
                mDensity = BaseSuggestionViewProperties.Density.SEMICOMPACT;
                mSuggestionSizePx = mContext.getResources().getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_semicompact_height);
            } else {
                mDensity = BaseSuggestionViewProperties.Density.COMPACT;
                mSuggestionSizePx = mContext.getResources().getDimensionPixelSize(
                        R.dimen.omnibox_suggestion_compact_height);
            }
        }
    }

    @Override
    public int getMinimumViewHeight() {
        return mSuggestionSizePx;
    }

    /**
     * Specify SuggestionDrawableState for suggestion decoration.
     *
     * @param decoration SuggestionDrawableState object defining decoration for the suggestion.
     */
    protected void setSuggestionDrawableState(
            PropertyModel model, SuggestionDrawableState decoration) {
        model.set(BaseSuggestionViewProperties.ICON, decoration);
    }

    /**
     * Specify SuggestionDrawableState for action button.
     *
     * @param model Property model to update.
     * @param actions List of actions for the suggestion.
     */
    protected void setCustomActions(PropertyModel model, List<Action> actions) {
        model.set(BaseSuggestionViewProperties.ACTIONS, actions);
    }

    /**
     * Setup action icon base on the suggestion, either show query build arrow or switch to tab.
     *
     * @param model Property model to update.
     * @param suggestion Suggestion associated with the action button.
     * @param position The position of the button in the list.
     */
    protected void setTabSwitchOrRefineAction(
            PropertyModel model, OmniboxSuggestion suggestion, int position) {
        @DrawableRes
        int icon = 0;
        String iconString = null;
        Runnable action = null;
        if (suggestion.hasTabMatch()) {
            icon = R.drawable.switch_to_tab;
            iconString =
                    mContext.getResources().getString(R.string.accessibility_omnibox_switch_to_tab);
            action = () -> mSuggestionHost.onSwitchToTab(suggestion, position);
        } else {
            icon = R.drawable.btn_suggestion_refine;
            iconString = mContext.getResources().getString(
                    R.string.accessibility_omnibox_btn_refine, suggestion.getFillIntoEdit());
            action = () -> mSuggestionHost.onRefineSuggestion(suggestion);
        }
        setCustomActions(model,
                Arrays.asList(
                        new Action(SuggestionDrawableState.Builder.forDrawableRes(mContext, icon)
                                           .setLarge(true)
                                           .setAllowTint(true)
                                           .build(),
                                iconString, action)));
    }

    /**
     * Process the click event.
     *
     * @param suggestion Selected suggestion.
     * @param position Position of the suggestion on the list.
     */
    protected void onSuggestionClicked(@NonNull OmniboxSuggestion suggestion, int position) {
        mSuggestionHost.onSuggestionClicked(suggestion, position, suggestion.getUrl());
    }

    /**
     * Process the long-click event.
     *
     * @param suggestion Selected suggestion.
     * @param position Position of the suggestion on the list.
     */
    protected void onSuggestionLongClicked(@NonNull OmniboxSuggestion suggestion, int position) {
        mSuggestionHost.onSuggestionLongClicked(suggestion, position);
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        model.set(BaseSuggestionViewProperties.ON_CLICK,
                () -> onSuggestionClicked(suggestion, position));
        model.set(BaseSuggestionViewProperties.ON_LONG_CLICK,
                () -> onSuggestionLongClicked(suggestion, position));
        model.set(BaseSuggestionViewProperties.ON_FOCUS_VIA_SELECTION,
                () -> mSuggestionHost.setOmniboxEditingText(suggestion.getFillIntoEdit()));
        model.set(BaseSuggestionViewProperties.DENSITY, mDensity);
        setCustomActions(model, null);
    }

    /**
     * Apply In-Place highlight to matching sections of Suggestion text.
     *
     * @param text Suggestion text to apply highlight to.
     * @param classifications Classifications describing how to format text.
     * @return true, if at least one highlighted match section was found.
     */
    protected static boolean applyHighlightToMatchRegions(
            Spannable text, List<MatchClassification> classifications) {
        if (text == null || classifications == null) return false;

        boolean hasAtLeastOneMatch = false;
        for (int i = 0; i < classifications.size(); i++) {
            MatchClassification classification = classifications.get(i);
            if ((classification.style & MatchClassificationStyle.MATCH)
                    == MatchClassificationStyle.MATCH) {
                int matchStartIndex = classification.offset;
                int matchEndIndex;
                if (i == classifications.size() - 1) {
                    matchEndIndex = text.length();
                } else {
                    matchEndIndex = classifications.get(i + 1).offset;
                }
                matchStartIndex = Math.min(matchStartIndex, text.length());
                matchEndIndex = Math.min(matchEndIndex, text.length());

                hasAtLeastOneMatch = true;
                // Bold the part of the URL that matches the user query.
                text.setSpan(new StyleSpan(Typeface.BOLD), matchStartIndex, matchEndIndex,
                        Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }
        return hasAtLeastOneMatch;
    }

    /**
     * Fetch suggestion favicon, if one is available.
     * Updates icon decoration in supplied |model| if |url| is not null and points to an already
     * visited website.
     *
     * @param model Model representing current suggestion.
     * @param url Target URL the suggestion points to.
     * @param iconBridge A {@link LargeIconBridge} supplies site favicons.
     * @param onIconFetched Optional callback that will be invoked after successful fetch of a
     *         favicon.
     */
    protected void fetchSuggestionFavicon(PropertyModel model, GURL url, LargeIconBridge iconBridge,
            @Nullable Runnable onIconFetched) {
        if (url == null || iconBridge == null) return;

        iconBridge.getLargeIconForUrl(url, mDesiredFaviconWidthPx,
                (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault, int iconType) -> {
                    if (icon == null) return;

                    setSuggestionDrawableState(model,
                            SuggestionDrawableState.Builder.forBitmap(mContext, icon).build());
                    if (onIconFetched != null) {
                        onIconFetched.run();
                    }
                });
    }

    /**
     * @return Current context.
     */
    protected Context getContext() {
        return mContext;
    }
}
