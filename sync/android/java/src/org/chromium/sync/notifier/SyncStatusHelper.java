// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;


import android.accounts.Account;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SyncStatusObserver;
import android.os.StrictMode;

import com.google.common.annotations.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;

import javax.annotation.concurrent.NotThreadSafe;
import javax.annotation.concurrent.ThreadSafe;

/**
 * A helper class to handle the current status of sync for Chrome in Android settings.
 *
 * It also provides an observer to be used whenever Android sync settings change.
 *
 * To retrieve an instance of this class, call SyncStatusHelper.get(someContext).
 */
@ThreadSafe
public class SyncStatusHelper {

    /**
     * In-memory holder of the sync configurations for a given account. On each
     * access, updates the cache if the account has changed. This lazy-updating
     * model is appropriate as the account changes rarely but may not be known
     * when initially constructed. So long as we keep a single account, no
     * expensive calls to Android are made.
     */
    @NotThreadSafe
    private class CachedAccountSyncSettings {
        private Account mAccount;
        private boolean mSyncAutomatically;
        private int mIsSyncable;

        private void ensureSettingsAreForAccount(Account account) {
            assert account != null;
            if (account.equals(mAccount)) return;
            updateSyncSettingsForAccount(account);
        }

        public void updateSyncSettingsForAccount(Account account) {
            if (account == null) return;
            mAccount = account;

            StrictMode.ThreadPolicy oldPolicy = temporarilyAllowDiskWritesAndDiskReads();
            mSyncAutomatically = mSyncContentResolverWrapper.getSyncAutomatically(
                    account, mContractAuthority);
            mIsSyncable = mSyncContentResolverWrapper.getIsSyncable(account, mContractAuthority);
            StrictMode.setThreadPolicy(oldPolicy);
        }

        public boolean getSyncAutomatically(Account account) {
            ensureSettingsAreForAccount(account);
            return mSyncAutomatically;
        }

        public void setIsSyncable(Account account) {
            ensureSettingsAreForAccount(account);
            if (mIsSyncable == 0) return;

            mIsSyncable = 1;
            StrictMode.ThreadPolicy oldPolicy = temporarilyAllowDiskWritesAndDiskReads();
            mSyncContentResolverWrapper.setIsSyncable(account, mContractAuthority, 1);
            StrictMode.setThreadPolicy(oldPolicy);
        }

        public void setSyncAutomatically(Account account, boolean value) {
            ensureSettingsAreForAccount(account);
            if (mSyncAutomatically == value) return;

            mSyncAutomatically = value;
            StrictMode.ThreadPolicy oldPolicy = temporarilyAllowDiskWritesAndDiskReads();
            mSyncContentResolverWrapper.setSyncAutomatically(account, mContractAuthority, value);
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    public static final String AUTH_TOKEN_TYPE_SYNC = "chromiumsync";

    public static final String TAG = "SyncStatusHelper";

    /**
     * Lock for ensuring singleton instantiation across threads.
     */
    private static final Object INSTANCE_LOCK = new Object();

    private static SyncStatusHelper sSyncStatusHelper;

    private final String mContractAuthority;

    private final Context mApplicationContext;

    private final SyncContentResolverDelegate mSyncContentResolverWrapper;

    private boolean mCachedMasterSyncAutomatically;

    // Instantiation of SyncStatusHelper is guarded by a lock so volatile is unneeded.
    private final CachedAccountSyncSettings mCachedSettings = new CachedAccountSyncSettings();

    private final ObserverList<SyncSettingsChangedObserver> mObservers =
            new ObserverList<SyncSettingsChangedObserver>();

    /**
     * Provides notifications when Android sync settings have changed.
     */
    public interface SyncSettingsChangedObserver {
        public void syncSettingsChanged();
    }

    /**
     * @param context the context
     * @param syncContentResolverWrapper an implementation of SyncContentResolverWrapper
     */
    private SyncStatusHelper(Context context,
            SyncContentResolverDelegate syncContentResolverWrapper) {
        mApplicationContext = context.getApplicationContext();
        mSyncContentResolverWrapper = syncContentResolverWrapper;
        mContractAuthority =
                InvalidationController.get(mApplicationContext).getContractAuthority();

        updateMasterSyncAutomaticallySetting();

        mSyncContentResolverWrapper.addStatusChangeListener(
                ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS,
                new AndroidSyncSettingsChangedObserver());
    }

    private void updateMasterSyncAutomaticallySetting() {
        StrictMode.ThreadPolicy oldPolicy = temporarilyAllowDiskWritesAndDiskReads();
        synchronized (mCachedSettings) {
            mCachedMasterSyncAutomatically = mSyncContentResolverWrapper
                    .getMasterSyncAutomatically();
        }
        StrictMode.setThreadPolicy(oldPolicy);
    }

    /**
     * A factory method for the SyncStatusHelper.
     *
     * It is possible to override the SyncContentResolverWrapper to use in tests for the
     * instance of the SyncStatusHelper by calling overrideSyncStatusHelperForTests(...) with
     * your SyncContentResolverWrapper.
     *
     * @param context the ApplicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the SyncStatusHelper
     */
    public static SyncStatusHelper get(Context context) {
        synchronized (INSTANCE_LOCK) {
            if (sSyncStatusHelper == null) {
                sSyncStatusHelper = new SyncStatusHelper(context,
                        new SystemSyncContentResolverDelegate());
            }
        }
        return sSyncStatusHelper;
    }

    /**
     * Tests might want to consider overriding the context and SyncContentResolverWrapper so they
     * do not use the real ContentResolver in Android.
     *
     * @param context the context to use
     * @param syncContentResolverWrapper the SyncContentResolverWrapper to use
     */
    @VisibleForTesting
    public static void overrideSyncStatusHelperForTests(Context context,
            SyncContentResolverDelegate syncContentResolverWrapper) {
        synchronized (INSTANCE_LOCK) {
            if (sSyncStatusHelper != null) {
                throw new IllegalStateException("SyncStatusHelper already exists");
            }
            sSyncStatusHelper = new SyncStatusHelper(context, syncContentResolverWrapper);
        }
    }

    /**
     * Wrapper method for the ContentResolver.addStatusChangeListener(...) when we are only
     * interested in the settings type.
     */
    public void registerContentResolverObserver(SyncSettingsChangedObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Wrapper method for the ContentResolver.removeStatusChangeListener(...).
     */
    public void unregisterContentResolverObserver(SyncSettingsChangedObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Checks whether sync is currently enabled from Chrome for a given account.
     *
     * It checks both the master sync for the device, and Chrome sync setting for the given account.
     *
     * @param account the account to check if Chrome sync is enabled on.
     * @return true if sync is on, false otherwise
     */
    public boolean isSyncEnabled(Account account) {
        if (account == null) return false;
        synchronized (mCachedSettings) {
            return mCachedMasterSyncAutomatically && mCachedSettings.getSyncAutomatically(account);
        }
    }

    /**
     * Checks whether sync is currently enabled from Chrome for the currently signed in account.
     *
     * It checks both the master sync for the device, and Chrome sync setting for the given account.
     * If no user is currently signed in it returns false.
     *
     * @return true if sync is on, false otherwise
     */
    public boolean isSyncEnabled() {
        return isSyncEnabled(ChromeSigninController.get(mApplicationContext).getSignedInUser());
    }

    /**
     * Checks whether sync is currently enabled from Chrome for a given account.
     *
     * It checks only Chrome sync setting for the given account,
     * and ignores the master sync setting.
     *
     * @param account the account to check if Chrome sync is enabled on.
     * @return true if sync is on, false otherwise
     */
    public boolean isSyncEnabledForChrome(Account account) {
        if (account == null) return false;
        synchronized (mCachedSettings) {
            return mCachedSettings.getSyncAutomatically(account);
        }
    }

    /**
     * Checks whether the master sync flag for Android is currently set.
     *
     * @return true if the global master sync is on, false otherwise
     */
    public boolean isMasterSyncAutomaticallyEnabled() {
        synchronized (mCachedSettings) {
            return mCachedMasterSyncAutomatically;
        }
    }

    /**
     * Make sure Chrome is syncable, and enable sync.
     *
     * @param account the account to enable sync on
     */
    public void enableAndroidSync(Account account) {
        makeSyncable(account);

        synchronized (mCachedSettings) {
            mCachedSettings.setSyncAutomatically(account, true);
        }
    }

    /**
     * Disables Android Chrome sync
     *
     * @param account the account to disable Chrome sync on
     */
    public void disableAndroidSync(Account account) {
        synchronized (mCachedSettings) {
            mCachedSettings.setSyncAutomatically(account, false);
        }
    }

    /**
     * Register with Android Sync Manager. This is what causes the "Chrome" option to appear in
     * Settings -> Accounts / Sync .
     *
     * @param account the account to enable Chrome sync on
     */
    private void makeSyncable(Account account) {
        synchronized (mCachedSettings) {
            mCachedSettings.setIsSyncable(account);
        }

        StrictMode.ThreadPolicy oldPolicy = temporarilyAllowDiskWritesAndDiskReads();
        // Disable the syncability of Chrome for all other accounts. Don't use
        // our cache as we're touching many accounts that aren't signed in, so this saves
        // extra calls to Android sync configuration.
        Account[] googleAccounts = AccountManagerHelper.get(mApplicationContext).
                getGoogleAccounts();
        for (Account accountToSetNotSyncable : googleAccounts) {
            if (!accountToSetNotSyncable.equals(account) &&
                    mSyncContentResolverWrapper.getIsSyncable(
                            accountToSetNotSyncable, mContractAuthority) > 0) {
                mSyncContentResolverWrapper.setIsSyncable(accountToSetNotSyncable,
                        mContractAuthority, 0);
            }
        }
        StrictMode.setThreadPolicy(oldPolicy);
    }

    /**
     * Helper class to be used by observers whenever sync settings change.
     *
     * To register the observer, call SyncStatusHelper.registerObserver(...).
     */
    private class AndroidSyncSettingsChangedObserver implements SyncStatusObserver {
        @Override
        public void onStatusChanged(int which) {
            if (ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS == which) {
                // Sync settings have changed; update our in-memory caches
                updateMasterSyncAutomaticallySetting();
                synchronized (mCachedSettings) {
                    mCachedSettings.updateSyncSettingsForAccount(
                            ChromeSigninController.get(mApplicationContext).getSignedInUser());
                }

                // Notify anyone else that's interested
                for (SyncSettingsChangedObserver observer: mObservers) {
                    observer.syncSettingsChanged();
                }
            }
        }
    }

    /**
     * Sets a new StrictMode.ThreadPolicy based on the current one, but allows disk reads
     * and disk writes.
     *
     * The return value is the old policy, which must be applied after the disk access is finished,
     * by using StrictMode.setThreadPolicy(oldPolicy).
     *
     * @return the policy before allowing reads and writes.
     */
    private static StrictMode.ThreadPolicy temporarilyAllowDiskWritesAndDiskReads() {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.getThreadPolicy();
        StrictMode.ThreadPolicy.Builder newPolicy =
                new StrictMode.ThreadPolicy.Builder(oldPolicy);
        newPolicy.permitDiskReads();
        newPolicy.permitDiskWrites();
        StrictMode.setThreadPolicy(newPolicy.build());
        return oldPolicy;
    }
}
