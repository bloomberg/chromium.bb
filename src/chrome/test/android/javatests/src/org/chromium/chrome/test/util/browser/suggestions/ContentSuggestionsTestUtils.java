// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.suggestions;

import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

/** Utilities to make testing content suggestions code easier. */
public final class ContentSuggestionsTestUtils {
    private ContentSuggestionsTestUtils() {}

    public static SnippetArticle createDummySuggestion(
            @CategoryInt int category, String suffix, boolean isVideoSuggestion) {
        return new SnippetArticle(category, "https://site.com/url" + suffix, "title" + suffix,
                "pub" + suffix, "https://site.com/url" + suffix, 0, 0, 0, isVideoSuggestion,
                /* thumbnailDominantColor = */ null);
    }

    public static SnippetArticle createDummySuggestion(@CategoryInt int category) {
        return createDummySuggestion(category, "", false);
    }

    public static List<SnippetArticle> createDummySuggestions(
            int count, @CategoryInt int category, String suffix) {
        List<SnippetArticle> suggestions = new ArrayList<>();
        for (int index = 0; index < count; index++) {
            suggestions.add(createDummySuggestion(category, suffix + index, false));
        }
        return suggestions;
    }

    public static List<SnippetArticle> createDummySuggestions(
            int count, @CategoryInt int category) {
        return createDummySuggestions(count, category, "");
    }

    /** Registers an empty category according to the provided category info. */
    public static void registerCategory(
            FakeSuggestionsSource suggestionsSource, @CategoryInt int category) {
        registerCategory(suggestionsSource, category, 0);
    }

    /**
     * Registers a category according to the provided category info.
     * @return the suggestions added to the newly registered category.
     */
    public static List<SnippetArticle> registerCategory(FakeSuggestionsSource suggestionsSource,
            @CategoryInt int category, int suggestionCount) {
        // Important: showIfEmpty flag to true.
        SuggestionsCategoryInfo categoryInfo =
                new CategoryInfoBuilder(category)
                        .withAction(ContentSuggestionsAdditionalAction.FETCH)
                        .showIfEmpty()
                        .build();
        return registerCategory(suggestionsSource, categoryInfo, suggestionCount);
    }

    /**
     * Registers a category that has a reload action and is shown if empty.
     * @return the suggestions added to the newly registered category.
     */
    public static List<SnippetArticle> registerCategory(FakeSuggestionsSource suggestionsSource,
            SuggestionsCategoryInfo categoryInfo, int suggestionCount) {
        // FakeSuggestionSource does not provide suggestions if the category's status is not
        // AVAILABLE.
        suggestionsSource.setStatusForCategory(
                categoryInfo.getCategory(), CategoryStatus.AVAILABLE);
        suggestionsSource.setInfoForCategory(categoryInfo.getCategory(), categoryInfo);

        if (suggestionCount == 0) return Collections.emptyList();

        List<SnippetArticle> suggestions =
                createDummySuggestions(suggestionCount, categoryInfo.getCategory());
        suggestionsSource.setSuggestionsForCategory(categoryInfo.getCategory(), suggestions);
        return suggestions;
    }

    /**
     * Uses the builder pattern to simplify constructing category info objects for tests.
     */
    public static class CategoryInfoBuilder {
        @CategoryInt
        private final int mCategory;
        private int mAdditionalAction;
        private boolean mShowIfEmpty;
        private String mTitle = "";
        private String mNoSuggestionsMessage = "";
        @ContentSuggestionsCardLayout
        private int mCardLayout = ContentSuggestionsCardLayout.FULL_CARD;

        public CategoryInfoBuilder(@CategoryInt int category) {
            mCategory = category;
        }

        public CategoryInfoBuilder withAction(@ContentSuggestionsAdditionalAction int action) {
            mAdditionalAction = action;
            return this;
        }
        public CategoryInfoBuilder showIfEmpty() {
            mShowIfEmpty = true;
            return this;
        }

        public CategoryInfoBuilder withTitle(String title) {
            mTitle = title;
            return this;
        }

        public CategoryInfoBuilder withNoSuggestionsMessage(String message) {
            mNoSuggestionsMessage = message;
            return this;
        }

        public CategoryInfoBuilder withCardLayout(@ContentSuggestionsCardLayout int cardLayout) {
            mCardLayout = cardLayout;
            return this;
        }

        public SuggestionsCategoryInfo build() {
            return new SuggestionsCategoryInfo(mCategory, mTitle, mCardLayout, mAdditionalAction,
                    mShowIfEmpty, mNoSuggestionsMessage);
        }
    }

    /** Helper method to print the current state of a node. */
    public static String stringify(RecyclerViewAdapter.Delegate root) {
        final StringBuilder stringBuilder = new StringBuilder();

        for (int i = 0; i < root.getItemCount(); i++) {
            stringBuilder.append(
                    String.format(Locale.US, "%s - %s%n", i, root.describeItemForTesting(i)));
        }

        return stringBuilder.toString();
    }
}
