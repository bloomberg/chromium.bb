// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.test.util;


import android.accounts.Account;
import android.content.ContentResolver;
import android.content.SyncStatusObserver;
import android.os.AsyncTask;

import junit.framework.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.sync.notifier.SyncContentResolverDelegate;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;


/**
 * Mock implementation of the SyncContentResolverWrapper.
 *
 * This implementation only supports status change listeners for the type
 * SYNC_OBSERVER_TYPE_SETTINGS.
 */
public class MockSyncContentResolverDelegate implements SyncContentResolverDelegate {

    private final Map<String, Boolean> mSyncAutomaticallyMap;

    private final Set<AsyncSyncStatusObserver> mObservers;

    private boolean mMasterSyncAutomatically;

    private Semaphore mPendingObserverCount;

    public MockSyncContentResolverDelegate() {
        mSyncAutomaticallyMap = new HashMap<String, Boolean>();
        mObservers = new HashSet<AsyncSyncStatusObserver>();
    }

    @Override
    public Object addStatusChangeListener(int mask, SyncStatusObserver callback) {
        if (mask != ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS) {
            throw new IllegalArgumentException("This implementation only supports "
                    + "ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS as the mask");
        }
        AsyncSyncStatusObserver asyncSyncStatusObserver = new AsyncSyncStatusObserver(callback);
        synchronized (mObservers) {
            mObservers.add(asyncSyncStatusObserver);
        }
        return asyncSyncStatusObserver;
    }

    @Override
    public void removeStatusChangeListener(Object handle) {
        synchronized (mObservers) {
            mObservers.remove(handle);
        }
    }

    @Override
    public void setMasterSyncAutomatically(boolean sync) {
        if (mMasterSyncAutomatically == sync) return;

        mMasterSyncAutomatically = sync;
        notifyObservers();
    }

    @Override
    public boolean getMasterSyncAutomatically() {
        return mMasterSyncAutomatically;
    }

    @Override
    public boolean getSyncAutomatically(Account account, String authority) {
        String key = createKey(account, authority);
        synchronized (mSyncAutomaticallyMap) {
            return mSyncAutomaticallyMap.containsKey(key) && mSyncAutomaticallyMap.get(key);
        }
    }

    @Override
    public void setSyncAutomatically(Account account, String authority, boolean sync) {
        String key = createKey(account, authority);
        synchronized (mSyncAutomaticallyMap) {
            if (!mSyncAutomaticallyMap.containsKey(key)) {
                throw new IllegalArgumentException("Account " + account +
                        " is not syncable for authority " + authority +
                        ". Can not set sync state to " + sync);
            }
            if (mSyncAutomaticallyMap.get(key) == sync) return;
            mSyncAutomaticallyMap.put(key, sync);
        }
        notifyObservers();
    }

    @Override
    public void setIsSyncable(Account account, String authority, int syncable) {
        String key = createKey(account, authority);

        synchronized (mSyncAutomaticallyMap) {
            switch (syncable) {
                case 0:
                    if (!mSyncAutomaticallyMap.containsKey(key)) return;

                    mSyncAutomaticallyMap.remove(key);
                    break;
                case 1:
                    if (mSyncAutomaticallyMap.containsKey(key)) return;

                    mSyncAutomaticallyMap.put(key, false);
                    break;
                default:
                    throw new IllegalArgumentException("Unable to understand syncable argument: " +
                            syncable);
            }
        }
        notifyObservers();
    }

    @Override
    public int getIsSyncable(Account account, String authority) {
        synchronized (mSyncAutomaticallyMap) {
            final Boolean isSyncable = mSyncAutomaticallyMap.get(createKey(account, authority));
            if (isSyncable == null) {
                return -1;
            }
            return isSyncable ? 1 : 0;
        }
    }

    private static String createKey(Account account, String authority) {
        return account.name + "@@@" + account.type + "@@@" + authority;
    }

    private void notifyObservers() {
        synchronized (mObservers) {
            mPendingObserverCount = new Semaphore(1 - mObservers.size());
            for (AsyncSyncStatusObserver observer : mObservers) {
                observer.notifyObserverAsync(mPendingObserverCount);
            }
        }
    }

    /**
     * Blocks until the last notification has been issued to all registered observers.
     * Note that if an observer is removed while a notification is being handled this can
     * fail to return correctly.
     *
     * @throws InterruptedException
     */
    public void waitForLastNotificationCompleted() throws InterruptedException {
        Assert.assertTrue("Timed out waiting for notifications to complete.",
                mPendingObserverCount.tryAcquire(5, TimeUnit.SECONDS));
    }

    private static class AsyncSyncStatusObserver {

        private final SyncStatusObserver mSyncStatusObserver;

        private AsyncSyncStatusObserver(SyncStatusObserver syncStatusObserver) {
            mSyncStatusObserver = syncStatusObserver;
        }

        private void notifyObserverAsync(final Semaphore pendingObserverCount) {
            if (ThreadUtils.runningOnUiThread()) {
                new AsyncTask<Void, Void, Void>() {
                    @Override
                    protected Void doInBackground(Void... params) {
                        mSyncStatusObserver.onStatusChanged(
                                ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS);
                        return null;
                    }

                    @Override
                    protected void onPostExecute(Void result) {
                        pendingObserverCount.release();
                    }
                }.execute();
            } else {
                mSyncStatusObserver.onStatusChanged(
                        ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS);
                pendingObserverCount.release();
            }
        }
    }
}
