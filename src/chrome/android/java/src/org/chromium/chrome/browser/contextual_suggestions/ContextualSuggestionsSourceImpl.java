// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsBridge.ContextualSuggestionsResult;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.ntp.snippets.EmptySuggestionsSource;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/**
 * Implementation of {@link ContextualSuggestionsSource}.
 */
class ContextualSuggestionsSourceImpl
        extends EmptySuggestionsSource implements ContextualSuggestionsSource {
    private ContextualSuggestionsBridge mBridge;
    private ImageFetcher mImageFetcher;

    /**
     * Creates a ContextualSuggestionsSource for getting contextual suggestions for the current
     * user.
     *
     * @param profile Profile of the user.
     */
    public ContextualSuggestionsSourceImpl(Profile profile) {
        mBridge = new ContextualSuggestionsBridge(profile);
        mImageFetcher = ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.DISK_CACHE_ONLY);
    }

    @Override
    public void destroy() {
        mBridge.destroy();
        mBridge = null;
        mImageFetcher.destroy();
        mImageFetcher = null;
    }

    @Override
    public void fetchSuggestionImage(SnippetArticle suggestion, Callback<Bitmap> callback) {
        String url = mBridge.getImageUrl(suggestion);
        mImageFetcher.fetchImage(
                url, ImageFetcher.CONTEXTUAL_SUGGESTIONS_UMA_CLIENT_NAME, callback);
    }

    @Override
    public void fetchContextualSuggestionImage(
            SnippetArticle suggestion, Callback<Bitmap> callback) {
        String url = mBridge.getImageUrl(suggestion);
        if (url == null) {
            PostTask.postTask(new TaskTraits(), () -> callback.onResult(null));
            return;
        }

        mImageFetcher.fetchImage(
                url, ImageFetcher.CONTEXTUAL_SUGGESTIONS_UMA_CLIENT_NAME, callback);
    }

    @Override
    public void fetchSuggestionFavicon(SnippetArticle suggestion, int minimumSizePx,
            int desiredSizePx, Callback<Bitmap> callback) {
        String url = mBridge.getFaviconUrl(suggestion);
        if (url == null) {
            PostTask.postTask(new TaskTraits(), () -> callback.onResult(null));
            return;
        }

        mImageFetcher.fetchImage(url, ImageFetcher.CONTEXTUAL_SUGGESTIONS_UMA_CLIENT_NAME,
                desiredSizePx, desiredSizePx, callback);
    }

    @Override
    public void fetchSuggestions(String url, Callback<ContextualSuggestionsResult> callback) {
        mBridge.fetchSuggestions(url, callback);
    }

    @Override
    public void reportEvent(WebContents webContents, @ContextualSuggestionsEvent int eventId) {
        mBridge.reportEvent(webContents, eventId);
    }

    @Override
    public void clearState() {
        mBridge.clearState();
    }

    @VisibleForTesting
    ContextualSuggestionsSourceImpl(ContextualSuggestionsBridge bridge, ImageFetcher imageFetcher) {
        mBridge = bridge;
        mImageFetcher = imageFetcher;
    }
}
