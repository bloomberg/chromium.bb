// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;

import com.google.common.collect.Lists;
import com.google.ipc.invalidation.external.client.types.AckHandle;
import com.google.ipc.invalidation.external.client.types.ObjectId;

import android.accounts.Account;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;

import java.util.List;

/**
 * Subclass of {@link InvalidationService} that captures events and allows controlling
 * whether or not Chrome is in the foreground and sync is enabled.
 *
 * @author dsmyers@google.com (Daniel Myers)
 */
public class TestableInvalidationService extends InvalidationService {
    /** Object ids given to {@link #register}, one list element per call. */
    final List<List<ObjectId>> mRegistrations = Lists.newArrayList();

    /** Object ids given to {@link #unregister}, one list element per call. */
    final List<List<ObjectId>> mUnregistrations = Lists.newArrayList();

    /** Intents given to {@link #startService}. */
    final List<Intent> mStartedServices = Lists.newArrayList();

    /** Bundles given to {@link #requestSyncFromContentResolver}. */
    final List<Bundle> mRequestedSyncs = Lists.newArrayList();

    final List<byte[]> mAcknowledgements = Lists.newArrayList();

    /** Whether Chrome is in the foreground. */
    private boolean mIsChromeInForeground = false;

    /** Whether sync is enabled. */
    private boolean mIsSyncEnabled = false;

    public TestableInvalidationService() {
    }

    @Override
    public void acknowledge(byte[] ackHandle) {
        mAcknowledgements.add(ackHandle);
    }

    @Override
    public void register(byte[] clientId, Iterable<ObjectId> objectIds) {
        mRegistrations.add(Lists.newArrayList(objectIds));
        super.register(clientId, objectIds);
    }

    @Override
    public void unregister(byte[] clientId, Iterable<ObjectId> objectIds) {
        mUnregistrations.add(Lists.newArrayList(objectIds));
        super.unregister(clientId, objectIds);
    }

    @Override
    public ComponentName startService(Intent intent) {
        mStartedServices.add(intent);
        return super.startService(intent);
    }

    @Override
    public void requestSyncFromContentResolver(Bundle bundle, Account account,
            String contractAuthority) {
        mRequestedSyncs.add(bundle);
        super.requestSyncFromContentResolver(bundle, account, contractAuthority);
    }

    @Override
    boolean isChromeInForeground() {
        return mIsChromeInForeground;
    }

    @Override
    boolean isSyncEnabled() {
        return mIsSyncEnabled;
    }

    /**
     * Sets the variables used to control whether or not a notification client should be running.
     * @param isChromeInForeground whether Chrome is in the foreground
     * @param isSyncEnabled whether sync is enabled
     */
    void setShouldRunStates(boolean isChromeInForeground, boolean isSyncEnabled) {
        this.mIsChromeInForeground = isChromeInForeground;
        this.mIsSyncEnabled = isSyncEnabled;
    }
}
