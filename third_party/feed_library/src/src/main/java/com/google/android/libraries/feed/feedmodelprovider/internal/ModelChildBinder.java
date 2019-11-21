// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.feedmodelprovider.internal;

import com.google.android.libraries.feed.api.internal.common.PayloadWithId;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelChild.Type;
import com.google.android.libraries.feed.api.internal.sessionmanager.FeedSessionManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.time.TimingUtils;
import com.google.android.libraries.feed.common.time.TimingUtils.ElapsedTimeTracker;
import com.google.search.now.feed.client.StreamDataProto.StreamPayload;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Bind the {@link StreamPayload} objects to the {@link ModelChild} instances. If the model child is
 * UNBOUND, we will bind the initial type. For FEATURE model child instances we will update the
 * data.
 */
public final class ModelChildBinder {
    private static final String TAG = "ModelChildBinder";

    private final FeedSessionManager feedSessionManager;
    private final CursorProvider cursorProvider;
    private final TimingUtils timingUtils;

    public ModelChildBinder(FeedSessionManager feedSessionManager, CursorProvider cursorProvider,
            TimingUtils timingUtils) {
        this.feedSessionManager = feedSessionManager;
        this.cursorProvider = cursorProvider;
        this.timingUtils = timingUtils;
    }

    public boolean bindChildren(List<UpdatableModelChild> childrenToBind) {
        ElapsedTimeTracker timeTracker = timingUtils.getElapsedTimeTracker(TAG);
        Map<String, UpdatableModelChild> bindingChildren = new HashMap<>();
        List<String> contentIds = new ArrayList<>();
        for (UpdatableModelChild child : childrenToBind) {
            String key = child.getContentId();
            contentIds.add(key);
            bindingChildren.put(key, child);
        }

        Result<List<PayloadWithId>> results = feedSessionManager.getStreamFeatures(contentIds);
        if (!results.isSuccessful()) {
            // If we failed, it's likely that this ModelProvider will soon be invalidated by the
            // FeedSessionManager
            Logger.e(TAG, "Unable to get the stream features.");
            return false;
        }
        List<PayloadWithId> payloads = results.getValue();
        if (contentIds.size() > payloads.size()) {
            Logger.e(TAG, "Didn't find all of the unbound content, found %s, expected %s",
                    payloads.size(), contentIds.size());
        }
        for (PayloadWithId childPayload : payloads) {
            String key = childPayload.contentId;
            UpdatableModelChild child = bindingChildren.get(key);
            if (child != null) {
                StreamPayload payload = childPayload.payload;
                if (child.getType() == Type.UNBOUND) {
                    if (payload.hasStreamFeature()) {
                        child.bindFeature(new UpdatableModelFeature(
                                payload.getStreamFeature(), cursorProvider));
                    } else if (payload.hasStreamToken()) {
                        child.bindToken(new UpdatableModelToken(payload.getStreamToken(), false));
                        continue;
                    } else {
                        Logger.e(TAG, "Unsupported Payload Type");
                    }
                    child.updateFeature(childPayload.payload);
                } else {
                    child.updateFeature(payload);
                }
            }
        }
        // TODO: log an error if any children are still left unbounded.
        timeTracker.stop("", "bindingChildren", "childrenToBind", childrenToBind.size());
        return true;
    }
}
