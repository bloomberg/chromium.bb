// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.host.storage.ContentMutation;
import com.google.android.libraries.feed.api.host.storage.ContentStorage;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * Implementation of {@link ContentStorage} that persisits data on native side.
 */
public class FeedContentStorage implements ContentStorage {
    private FeedContentBridge mFeedContentBridge;

    /**
     * Creates a {@link FeedContentStorage} for storing content for the current user.
     *
     * @param profile {@link Profile} of the user we are rendering the Feed for.
     */
    public FeedContentStorage(Profile profile) {
        mFeedContentBridge = new FeedContentBridge(profile);
    }

    /**
     * Creates a {@link FeedContentStorage} for testing.
     *
     * @param bridge {@link FeedContentBridge} implementation can handle content storage request.
     */
    @VisibleForTesting
    public FeedContentStorage(FeedContentBridge bridge) {
        mFeedContentBridge = bridge;
    }

    /** Cleans up {@link FeedContentStorage}. */
    public void destroy() {
        assert mFeedContentBridge != null;
        mFeedContentBridge.destroy();
        mFeedContentBridge = null;
    }

    @Override
    public void get(List<String> keys, Consumer < Result < Map<String, byte[]>>> consumer) {
        if (mFeedContentBridge == null) {
            consumer.accept(Result.failure());
        } else {
            mFeedContentBridge.loadContent(keys,
                    (Map<String, byte[]> data)
                            -> consumer.accept(Result.success(data)),
                    (Void ignored) -> consumer.accept(Result.failure()));
        }
    }

    @Override
    public void getAll(String prefix, Consumer < Result < Map<String, byte[]>>> consumer) {
        if (mFeedContentBridge == null) {
            consumer.accept(Result.failure());
        } else {
            mFeedContentBridge.loadContentByPrefix(prefix,
                    (Map<String, byte[]> data)
                            -> consumer.accept(Result.success(data)),
                    (Void ignored) -> consumer.accept(Result.failure()));
        }
    }

    @Override
    public void commit(ContentMutation mutation, Consumer<CommitResult> consumer) {
        if (mFeedContentBridge == null) {
            consumer.accept(CommitResult.FAILURE);
        } else {
            mFeedContentBridge.commitContentMutation(mutation,
                    (Boolean result)
                            -> consumer.accept(
                                    result ? CommitResult.SUCCESS : CommitResult.FAILURE));
        }
    }

    @Override
    public void getAllKeys(Consumer < Result < List<String>>> consumer) {
        if (mFeedContentBridge == null) {
            consumer.accept(Result.failure());
        } else {
            mFeedContentBridge.loadAllContentKeys(
                    (String[] keys)
                            -> consumer.accept(Result.success(Arrays.asList(keys))),
                    (Void ignored) -> consumer.accept(Result.failure()));
        }
    }
}
