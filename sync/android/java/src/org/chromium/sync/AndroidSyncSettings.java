// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync;

import android.accounts.Account;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SyncStatusObserver;
import android.os.StrictMode;

import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.sync.signin.AccountManagerHelper;
import org.chromium.sync.signin.ChromeSigninController;

import javax.annotation.concurrent.NotThreadSafe;
import javax.annotation.concurrent.ThreadSafe;

/**
 * A helper class to handle the current status of sync for Chrome in Android settings.
 *
 * It also provides an observer to be used whenever Android sync settings change.
 *
 * To retrieve an instance of this class, call AndroidSyncSettings.get(someContext).
 *
 * All new public methods MUST call notifyObservers at the end.
 */
@ThreadSafe
public class AndroidSyncSettings {

    /**
     * In-memory holder of the sync configurations for a given account. On each
     * access, updates the cache if the account has changed. This lazy-updating
     * model is appropriate as the account changes rarely but may not be known
     * when initially constructed. So long as we keep a single account, no
     * expensive calls to Android are made.
     */
    @NotThreadSafe
    @VisibleForTesting
    public static class CachedAccountSyncSettings {
        private final String mContractAuthority;
        private final SyncContentResolverDelegate mSyncContentResolverDelegate;
        private Account mAccount;
        private boolean mDidUpdate;
        private boolean mSyncAutomatically;
        private int mIsSyncable;

        public CachedAccountSyncSettings(String contractAuthority,
                SyncContentResolverDelegate contentResolverWrapper) {
            mContractAuthority = contractAuthority;
            mSyncContentResolverDelegate = contentResolverWrapper;
        }

        private void ensureSettingsAreForAccount(Account account) {
            assert account != null;
            if (account.equals(mAccount)) return;
            updateSyncSettingsForAccount(account);
            mDidUpdate = true;
        }

        public void clearUpdateStatus() {
            mDidUpdate = false;
        }

        public boolean getDidUpdateStatus() {
            return mDidUpdate;
        }

        // Calling this method may have side-effects.
        public boolean getSyncAutomatically(Account account) {
            ensureSettingsAreForAccount(account);
            return mSyncAutomatically;
        }

        public void updateSyncSettingsForAccount(Account account) {
            if (account == null) return;
            updateSyncSettingsForAccountInternal(account);
        }

        public void setIsSyncable(Account account) {
            ensureSettingsAreForAccount(account);
            if (mIsSyncable == 1) return;
            setIsSyncableInternal(account);
        }

        public void setSyncAutomatically(Account account, boolean value) {
            ensureSettingsAreForAccount(account);
            if (mSyncAutomatically == value) return;
            setSyncAutomaticallyInternal(account, value);
        }

        @VisibleForTesting
        protected void updateSyncSettingsForAccountInternal(Account account) {
            // Null check here otherwise Findbugs complains.
            if (account == null) return;

            boolean oldSyncAutomatically = mSyncAutomatically;
            int oldIsSyncable = mIsSyncable;
            Account oldAccount = mAccount;

            mAccount = account;

            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
            mSyncAutomatically = mSyncContentResolverDelegate.getSyncAutomatically(
                    account, mContractAuthority);
            mIsSyncable = mSyncContentResolverDelegate.getIsSyncable(account, mContractAuthority);
            StrictMode.setThreadPolicy(oldPolicy);
            mDidUpdate = (oldIsSyncable != mIsSyncable)
                || (oldSyncAutomatically != mSyncAutomatically)
                || (!account.equals(oldAccount));
        }

        @VisibleForTesting
        protected void setIsSyncableInternal(Account account) {
            mIsSyncable = 1;
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
            mSyncContentResolverDelegate.setIsSyncable(account, mContractAuthority, 1);
            StrictMode.setThreadPolicy(oldPolicy);
            mDidUpdate = true;
        }

        @VisibleForTesting
        protected void setSyncAutomaticallyInternal(Account account, boolean value) {
            mSyncAutomatically = value;
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
            mSyncContentResolverDelegate.setSyncAutomatically(account, mContractAuthority, value);
            StrictMode.setThreadPolicy(oldPolicy);
            mDidUpdate = true;
        }
    }

    public static final String TAG = "AndroidSyncSettings";

    /**
     * Lock for ensuring singleton instantiation across threads.
     */
    private static final Object INSTANCE_LOCK = new Object();

    private static AndroidSyncSettings sAndroidSyncSettings;

    private final String mContractAuthority;

    private final Context mApplicationContext;

    private final SyncContentResolverDelegate mSyncContentResolverDelegate;

    private boolean mCachedMasterSyncAutomatically;

    // Instantiation of AndroidSyncSettings is guarded by a lock so volatile is unneeded.
    private CachedAccountSyncSettings mCachedSettings;

    private final ObserverList<AndroidSyncSettingsObserver> mObservers =
            new ObserverList<AndroidSyncSettingsObserver>();

    /**
     * Provides notifications when Android sync settings have changed.
     */
    public interface AndroidSyncSettingsObserver {
        public void androidSyncSettingsChanged();
    }

    /**
     * @param context the context
     * @param syncContentResolverDelegate an implementation of {@link SyncContentResolverDelegate}.
     */
    private AndroidSyncSettings(Context context,
            SyncContentResolverDelegate syncContentResolverDelegate,
            CachedAccountSyncSettings cachedAccountSettings) {
        mApplicationContext = context.getApplicationContext();
        mSyncContentResolverDelegate = syncContentResolverDelegate;
        mContractAuthority = getContractAuthority();
        mCachedSettings = cachedAccountSettings;

        updateMasterSyncAutomaticallySetting();

        mSyncContentResolverDelegate.addStatusChangeListener(
                ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS,
                new AndroidSyncSettingsChangedObserver());
    }

    private void updateMasterSyncAutomaticallySetting() {
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        synchronized (mCachedSettings) {
            mCachedMasterSyncAutomatically = mSyncContentResolverDelegate
                    .getMasterSyncAutomatically();
        }
        StrictMode.setThreadPolicy(oldPolicy);
    }

    /**
     * A factory method for the AndroidSyncSettings.
     *
     * It is possible to override the {@link SyncContentResolverDelegate} to use in tests for the
     * instance of the AndroidSyncSettings by calling overrideAndroidSyncSettingsForTests(...) with
     * your {@link SyncContentResolverDelegate}.
     *
     * @param context the ApplicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the AndroidSyncSettings
     */
    public static AndroidSyncSettings get(Context context) {
        synchronized (INSTANCE_LOCK) {
            if (sAndroidSyncSettings == null) {
                SyncContentResolverDelegate contentResolverDelegate =
                        new SystemSyncContentResolverDelegate();
                CachedAccountSyncSettings cache = new CachedAccountSyncSettings(
                        context.getPackageName(), contentResolverDelegate);
                sAndroidSyncSettings = new AndroidSyncSettings(
                        context, contentResolverDelegate, cache);
            }
        }
        return sAndroidSyncSettings;
    }

    /**
     * Tests might want to consider overriding the context and {@link SyncContentResolverDelegate}
     * so they do not use the real ContentResolver in Android.
     *
     * @param context the context to use
     * @param syncContentResolverDelegate the {@link SyncContentResolverDelegate} to use
     */
    @VisibleForTesting
    public static void overrideAndroidSyncSettingsForTests(Context context,
            SyncContentResolverDelegate syncContentResolverDelegate,
            CachedAccountSyncSettings cachedAccountSettings) {
        synchronized (INSTANCE_LOCK) {
            if (sAndroidSyncSettings != null) {
                throw new IllegalStateException("AndroidSyncSettings already exists");
            }
            sAndroidSyncSettings = new AndroidSyncSettings(context, syncContentResolverDelegate,
                    cachedAccountSettings);
        }
    }

    @VisibleForTesting
    public static void overrideAndroidSyncSettingsForTests(Context context,
            SyncContentResolverDelegate syncContentResolverDelegate) {
        CachedAccountSyncSettings cachedAccountSettings = new CachedAccountSyncSettings(
                context.getPackageName(), syncContentResolverDelegate);
        overrideAndroidSyncSettingsForTests(context, syncContentResolverDelegate,
                cachedAccountSettings);
    }

    /**
     * Returns the contract authority to use when requesting sync.
     */
    public String getContractAuthority() {
        return mApplicationContext.getPackageName();
    }

    /**
     * Add a new AndroidSyncSettingsObserver.
     */
    public void registerObserver(AndroidSyncSettingsObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an AndroidSyncSettingsObserver that was previously added.
     */
    public void unregisterObserver(AndroidSyncSettingsObserver observer) {
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
        boolean returnValue;
        synchronized (mCachedSettings) {
            returnValue = mCachedMasterSyncAutomatically
                    && mCachedSettings.getSyncAutomatically(account);
        }

        notifyObserversIfAccountSettingsChanged();
        return returnValue;
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
    public boolean isChromeSyncEnabled(Account account) {
        if (account == null) return false;

        boolean returnValue;
        synchronized (mCachedSettings) {
            returnValue = mCachedSettings.getSyncAutomatically(account);
        }

        notifyObserversIfAccountSettingsChanged();
        return returnValue;
    }

    /**
     * Checks whether the master sync flag for Android is currently set.
     *
     * @return true if the global master sync is on, false otherwise
     */
    public boolean isMasterSyncEnabled() {
        synchronized (mCachedSettings) {
            return mCachedMasterSyncAutomatically;
        }
    }

    /**
     * Make sure Chrome is syncable, and enable sync.
     *
     * @param account the account to enable sync on
     */
    public void enableChromeSync(Account account) {
        makeSyncable(account);

        synchronized (mCachedSettings) {
            mCachedSettings.setSyncAutomatically(account, true);
        }

        notifyObserversIfAccountSettingsChanged();
    }

    /**
     * Disables Android Chrome sync
     *
     * @param account the account to disable Chrome sync on
     */
    public void disableChromeSync(Account account) {
        synchronized (mCachedSettings) {
            mCachedSettings.setSyncAutomatically(account, false);
        }

        notifyObserversIfAccountSettingsChanged();
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

        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        // Disable the syncability of Chrome for all other accounts. Don't use
        // our cache as we're touching many accounts that aren't signed in, so this saves
        // extra calls to Android sync configuration.
        Account[] googleAccounts = AccountManagerHelper.get(mApplicationContext)
                .getGoogleAccounts();
        for (Account accountToSetNotSyncable : googleAccounts) {
            if (!accountToSetNotSyncable.equals(account)
                    && mSyncContentResolverDelegate.getIsSyncable(
                            accountToSetNotSyncable, mContractAuthority) > 0) {
                mSyncContentResolverDelegate.setIsSyncable(accountToSetNotSyncable,
                        mContractAuthority, 0);
            }
        }
        StrictMode.setThreadPolicy(oldPolicy);
    }

    /**
     * Helper class to be used by observers whenever sync settings change.
     *
     * To register the observer, call AndroidSyncSettings.registerObserver(...).
     */
    private class AndroidSyncSettingsChangedObserver implements SyncStatusObserver {
        @Override
        public void onStatusChanged(int which) {
            if (ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS == which) {
                // Sync settings have changed; update our in-memory caches
                synchronized (mCachedSettings) {
                    mCachedSettings.updateSyncSettingsForAccount(
                            ChromeSigninController.get(mApplicationContext).getSignedInUser());
                }

                boolean oldMasterSyncEnabled = isMasterSyncEnabled();
                updateMasterSyncAutomaticallySetting();
                boolean didMasterSyncChanged = oldMasterSyncEnabled != isMasterSyncEnabled();
                // Notify observers if MasterSync or account level settings change.
                if (didMasterSyncChanged || getAndClearDidUpdateStatus()) {
                    notifyObservers();
                }
            }
        }
    }

    private boolean getAndClearDidUpdateStatus() {
        boolean didGetStatusUpdate;
        synchronized (mCachedSettings) {
            didGetStatusUpdate = mCachedSettings.getDidUpdateStatus();
            mCachedSettings.clearUpdateStatus();
        }
        return didGetStatusUpdate;
    }

    private void notifyObserversIfAccountSettingsChanged() {
        if (getAndClearDidUpdateStatus()) {
            notifyObservers();
        }
    }

    private void notifyObservers() {
        for (AndroidSyncSettingsObserver observer : mObservers) {
            observer.androidSyncSettingsChanged();
        }
    }
}
