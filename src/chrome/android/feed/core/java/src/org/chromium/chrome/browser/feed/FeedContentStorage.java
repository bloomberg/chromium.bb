// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.host.storage.CommitResult;
import com.google.android.libraries.feed.host.storage.ContentMutation;
import com.google.android.libraries.feed.host.storage.ContentStorage;

import org.chromium.base.Callback;

import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * Implementation of {@link ContentStorage} that persisits data on native side.
 */
public class FeedContentStorage implements ContentStorage {
    private FeedContentBridge mFeedContentBridge;

    private static class StorageCallback<T> implements Callback<T> {
        private final Consumer<Result<T>> mConsumer;

        public StorageCallback(Consumer<Result<T>> consumer) {
            mConsumer = consumer;
        }

        @Override
        public void onResult(T data) {
            // TODO(gangwu): Need to handle failure case.
            mConsumer.accept(Result.success(data));
        }
    }

    private static class GetAllKeysCallback implements Callback<String[]> {
        private final Consumer < Result < List<String>>> mConsumer;

        public GetAllKeysCallback(Consumer < Result < List<String>>> consumer) {
            mConsumer = consumer;
        }

        @Override
        public void onResult(String[] keys) {
            // TODO(gangwu): Need to handle failure case.
            mConsumer.accept(Result.success(Arrays.asList(keys)));
        }
    }

    private static class CommitCallback implements Callback<Boolean> {
        private final Consumer<CommitResult> mConsumer;

        CommitCallback(Consumer<CommitResult> consumer) {
            mConsumer = consumer;
        }

        @Override
        public void onResult(Boolean result) {
            if (result) {
                mConsumer.accept(CommitResult.SUCCESS);
            } else {
                mConsumer.accept(CommitResult.FAILURE);
            }
        }
    }

    /**
     * Creates a {@link FeedContentStorage} for storing content for the current user.
     *
     * @param bridge {@link FeedContentBridge} implementation can handle content storage request.
     */
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
        assert mFeedContentBridge != null;
        mFeedContentBridge.loadContent(keys, new StorageCallback<Map<String, byte[]>>(consumer));
    }

    @Override
    public void getAll(String prefix, Consumer < Result < Map<String, byte[]>>> consumer) {
        assert mFeedContentBridge != null;
        mFeedContentBridge.loadContentByPrefix(
                prefix, new StorageCallback<Map<String, byte[]>>(consumer));
    }

    @Override
    public void commit(ContentMutation mutation, Consumer<CommitResult> consumer) {
        assert mFeedContentBridge != null;
        mFeedContentBridge.commitContentMutation(mutation, new CommitCallback(consumer));
    }

    @Override
    public void getAllKeys(Consumer < Result < List<String>>> consumer) {
        assert mFeedContentBridge != null;
        mFeedContentBridge.loadAllContentKeys(new GetAllKeysCallback(consumer));
    }
}
