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

import javax.annotation.concurrent.ThreadSafe;

/**
 * A helper class to handle the current status of sync for Chrome in Android settings.
 *
 * It also provides an observer to be used whenever Android sync settings change.
 *
 * To retrieve an instance of this class, call AndroidSyncSettings.get(someContext).
 *
 * This class must be initialized via updateAccount() on startup if the user is signed in.
 */
@ThreadSafe
public class AndroidSyncSettings {

    public static final String TAG = "AndroidSyncSettings";

    /**
     * Lock for ensuring singleton instantiation across threads.
     */
    private static final Object CLASS_LOCK = new Object();

    private static AndroidSyncSettings sAndroidSyncSettings;

    private final Object mLock = new Object();

    private final String mContractAuthority;

    private final Context mApplicationContext;

    private final SyncContentResolverDelegate mSyncContentResolverDelegate;

    private Account mAccount = null;

    private boolean mIsSyncable = false;

    private boolean mChromeSyncEnabled = false;

    private boolean mMasterSyncEnabled = false;

    private final ObserverList<AndroidSyncSettingsObserver> mObservers =
            new ObserverList<AndroidSyncSettingsObserver>();

    /**
     * Provides notifications when Android sync settings have changed.
     */
    public interface AndroidSyncSettingsObserver {
        public void androidSyncSettingsChanged();
    }

    /**
     * A factory method for the AndroidSyncSettings.
     *
     * It is possible to override the {@link SyncContentResolverDelegate} to use in tests for the
     * instance of the AndroidSyncSettings by calling overrideForTests(...) with
     * your {@link SyncContentResolverDelegate}.
     *
     * @param context the ApplicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the AndroidSyncSettings
     */
    public static AndroidSyncSettings get(Context context) {
        synchronized (CLASS_LOCK) {
            if (sAndroidSyncSettings == null) {
                SyncContentResolverDelegate contentResolver =
                        new SystemSyncContentResolverDelegate();
                sAndroidSyncSettings = new AndroidSyncSettings(context, contentResolver);
            }
        }
        return sAndroidSyncSettings;
    }

    @VisibleForTesting
    public static void overrideForTests(Context context,
            SyncContentResolverDelegate contentResolver) {
        synchronized (CLASS_LOCK) {
            sAndroidSyncSettings = new AndroidSyncSettings(context, contentResolver);
        }
    }

    /**
     * @param context the context
     * @param syncContentResolverDelegate an implementation of {@link SyncContentResolverDelegate}.
     */
    private AndroidSyncSettings(Context context,
            SyncContentResolverDelegate syncContentResolverDelegate) {
        mApplicationContext = context.getApplicationContext();
        mSyncContentResolverDelegate = syncContentResolverDelegate;
        mContractAuthority = getContractAuthority();

        updateCachedSettings();

        mSyncContentResolverDelegate.addStatusChangeListener(
                ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS,
                new AndroidSyncSettingsChangedObserver());
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
        return mMasterSyncEnabled && mChromeSyncEnabled;
    }

    /**
     * Checks whether sync is currently enabled from Chrome for a given account.
     *
     * It checks only Chrome sync setting for the given account,
     * and ignores the master sync setting.
     *
     * @return true if sync is on, false otherwise
     */
    public boolean isChromeSyncEnabled() {
        return mChromeSyncEnabled;
    }

    /**
     * Checks whether the master sync flag for Android is currently enabled.
     */
    public boolean isMasterSyncEnabled() {
        return mMasterSyncEnabled;
    }

    /**
     * Make sure Chrome is syncable, and enable sync.
     */
    public void enableChromeSync() {
        setChromeSyncEnabled(true);
    }

    /**
     * Disables Android Chrome sync
     */
    public void disableChromeSync() {
        setChromeSyncEnabled(false);
    }

    /**
     * Must be called when a new account is signed in.
     */
    public void updateAccount(Account account) {
        synchronized (mLock) {
            mAccount = account;
        }
        if (updateCachedSettings()) {
            notifyObservers();
        }
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
        synchronized (mLock) {
            mObservers.addObserver(observer);
        }
    }

    /**
     * Remove an AndroidSyncSettingsObserver that was previously added.
     */
    public void unregisterObserver(AndroidSyncSettingsObserver observer) {
        synchronized (mLock) {
            mObservers.removeObserver(observer);
        }
    }

    private void setChromeSyncEnabled(boolean value) {
        synchronized (mLock) {
            if (value == mChromeSyncEnabled || mAccount == null) return;
            ensureSyncable();
            mChromeSyncEnabled = value;

            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
            mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mContractAuthority, value);
            StrictMode.setThreadPolicy(oldPolicy);
        }
        notifyObservers();
    }

    /**
     * Ensure Chrome is registered with the Android Sync Manager.
     *
     * This is what causes the "Chrome" option to appear in Settings -> Accounts -> Sync .
     * This function must be called within a synchronized block.
     */
    private void ensureSyncable() {
        if (mIsSyncable || mAccount == null) return;

        mIsSyncable = true;

        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        mSyncContentResolverDelegate.setIsSyncable(mAccount, mContractAuthority, 1);

        // Disable the syncability of Chrome for all other accounts. Don't use
        // our cache as we're touching many accounts that aren't signed in, so this saves
        // extra calls to Android sync configuration.
        Account[] googleAccounts = AccountManagerHelper.get(mApplicationContext)
                .getGoogleAccounts();
        for (Account accountToSetNotSyncable : googleAccounts) {
            if (!accountToSetNotSyncable.equals(mAccount)
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
            if (which == ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS) {
                // Sync settings have changed; update our cached values.
                if (updateCachedSettings()) {
                    // If something actually changed, tell our observers.
                    notifyObservers();
                }
            }
        }
    }

    /**
     * Update the three cached settings from the content resolver.
     *
     * @return Whether either chromeSyncEnabled or masterSyncEnabled changed.
     */
    private boolean updateCachedSettings() {
        synchronized (mLock) {
            boolean oldChromeSyncEnabled = mChromeSyncEnabled;
            boolean oldMasterSyncEnabled = mMasterSyncEnabled;

            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
            if (mAccount != null) {
                mIsSyncable = mSyncContentResolverDelegate.getIsSyncable(
                        mAccount, mContractAuthority) == 1;
                mChromeSyncEnabled = mSyncContentResolverDelegate.getSyncAutomatically(
                        mAccount, mContractAuthority);
            } else {
                mIsSyncable = false;
                mChromeSyncEnabled = false;
            }
            mMasterSyncEnabled = mSyncContentResolverDelegate.getMasterSyncAutomatically();
            StrictMode.setThreadPolicy(oldPolicy);

            return oldChromeSyncEnabled != mChromeSyncEnabled
                || oldMasterSyncEnabled != mMasterSyncEnabled;
        }
    }

    private void notifyObservers() {
        for (AndroidSyncSettingsObserver observer : mObservers) {
            observer.androidSyncSettingsChanged();
        }
    }
}
