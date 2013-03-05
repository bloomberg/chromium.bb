// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier;


import android.accounts.Account;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.SyncStatusObserver;
import android.os.StrictMode;
import android.preference.PreferenceManager;
import android.util.Log;

import org.chromium.base.ObserverList;
import org.chromium.sync.signin.AccountManagerHelper;

import com.google.common.annotations.VisibleForTesting;

/**
 * A helper class to handle the current status of sync for Chrome in Android-land.
 *
 * It also has a helper class to be used by observers whenever sync settings change.
 *
 * To retrieve an instance of this class, call SyncStatusHelper.get(someContext).
 */
public class SyncStatusHelper {

    public interface Listener {
        /**
         * Called when the user signs out of Chrome.
         */
        void onClearSignedInUser();
    }

    // TODO(dsmyers): remove the downstream version of this constant.
    public static final String AUTH_TOKEN_TYPE_SYNC = "chromiumsync";

    @VisibleForTesting
    public static final String SIGNED_IN_ACCOUNT_KEY = "google.services.username";

    public static final String TAG = "SyncStatusHelper";

    private final Context mApplicationContext;

    private final SyncContentResolverDelegate mSyncContentResolverWrapper;

    private static final Object lock = new Object();

    private static SyncStatusHelper sSyncStatusHelper;

    private ObserverList<Listener> mListeners;

    /**
     * @param context the context
     * @param syncContentResolverWrapper an implementation of SyncContentResolverWrapper
     */
    private SyncStatusHelper(Context context,
            SyncContentResolverDelegate syncContentResolverWrapper) {
        mApplicationContext = context.getApplicationContext();
        mSyncContentResolverWrapper = syncContentResolverWrapper;
        mListeners = new ObserverList<Listener>();
    }

    /**
     * A factory method for the SyncStatusHelper.
     *
     * It is possible to override the SyncContentResolverWrapper to use in tests for the
     * instance of the SyncStatusHelper by calling overrideSyncStatusHelperForTests(...) with
     * your SyncContentResolverWrapper.
     *
     * @param context the ApplicationContext is retreived from the context used as an argument.
     * @return a singleton instance of the SyncStatusHelper
     */
    public static SyncStatusHelper get(Context context) {
        synchronized (lock) {
            if (sSyncStatusHelper == null) {
                Context applicationContext = context.getApplicationContext();
                sSyncStatusHelper = new SyncStatusHelper(applicationContext,
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
        synchronized (lock) {
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
    public Object registerContentResolverObserver(SyncStatusObserver observer) {
        return mSyncContentResolverWrapper.addStatusChangeListener(
                ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS, observer);
    }

    /**
     * Wrapper method for the ContentResolver.removeStatusChangeListener(...).
     */
    public void unregisterContentResolverObserver(Object observerHandle) {
        mSyncContentResolverWrapper.removeStatusChangeListener(observerHandle);
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
        StrictMode.ThreadPolicy oldPolicy = temporarilyAllowDiskWritesAndDiskReads();
        String contractAuthority =
                InvalidationController.get(mApplicationContext).getContractAuthority();
        boolean enabled = account != null &&
                mSyncContentResolverWrapper.getMasterSyncAutomatically() &&
                mSyncContentResolverWrapper.getSyncAutomatically(account, contractAuthority);
        StrictMode.setThreadPolicy(oldPolicy);
        return enabled;
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
        return isSyncEnabled(getSignedInUser());
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
        StrictMode.ThreadPolicy oldPolicy = temporarilyAllowDiskWritesAndDiskReads();
        String contractAuthority =
                InvalidationController.get(mApplicationContext).getContractAuthority();
        boolean enabled = account != null &&
                mSyncContentResolverWrapper.getSyncAutomatically(account, contractAuthority);
        StrictMode.setThreadPolicy(oldPolicy);
        return enabled;
    }

    /**
     * Checks whether the master sync flag for Android is currently set.
     *
     * @return true if the global master sync is on, false otherwise
     */
    public boolean isMasterSyncAutomaticallyEnabled() {
        StrictMode.ThreadPolicy oldPolicy = temporarilyAllowDiskWritesAndDiskReads();
        boolean enabled = mSyncContentResolverWrapper.getMasterSyncAutomatically();
        StrictMode.setThreadPolicy(oldPolicy);
        return enabled;
    }

    /**
     * Make sure Chrome is syncable, and enable sync.
     *
     * @param account the account to enable sync on
     */
    public void enableAndroidSync(Account account) {
        StrictMode.ThreadPolicy oldPolicy = temporarilyAllowDiskWritesAndDiskReads();
        makeSyncable(account);
        String contractAuthority =
                InvalidationController.get(mApplicationContext).getContractAuthority();
        if (!mSyncContentResolverWrapper.getSyncAutomatically(account, contractAuthority)) {
            mSyncContentResolverWrapper.setSyncAutomatically(account, contractAuthority, true);
        }
        StrictMode.setThreadPolicy(oldPolicy);
    }

    /**
     * Disables Android Chrome sync
     *
     * @param account the account to disable Chrome sync on
     */
    public void disableAndroidSync(Account account) {
        StrictMode.ThreadPolicy oldPolicy = temporarilyAllowDiskWritesAndDiskReads();
        String contractAuthority =
                InvalidationController.get(mApplicationContext).getContractAuthority();
        if (mSyncContentResolverWrapper.getSyncAutomatically(account, contractAuthority)) {
            mSyncContentResolverWrapper.setSyncAutomatically(account, contractAuthority, false);
        }
        StrictMode.setThreadPolicy(oldPolicy);
    }

    // TODO(nyquist) Move all these methods about signed in user to GoogleServicesManager.
    public Account getSignedInUser() {
        String syncAccountName = getSignedInAccountName();
        if (syncAccountName == null) {
            return null;
        }
        return AccountManagerHelper.createAccountFromName(syncAccountName);
    }

    public boolean isSignedIn() {
        return getSignedInAccountName() != null;
    }

    public void setSignedInAccountName(String accountName) {
        getPreferences().edit()
            .putString(SIGNED_IN_ACCOUNT_KEY, accountName)
            .apply();
    }

    public void clearSignedInUser() {
        Log.d(TAG, "Clearing user signed in to Chrome");
        setSignedInAccountName(null);

        for (Listener listener : mListeners) {
            listener.onClearSignedInUser();
        }
    }

    private String getSignedInAccountName() {
        return getPreferences().getString(SIGNED_IN_ACCOUNT_KEY, null);
    }

    /**
     * Register with Android Sync Manager. This is what causes the "Chrome" option to appear in
     * Settings -> Accounts / Sync .
     *
     * @param account the account to enable Chrome sync on
     */
    private void makeSyncable(Account account) {
        String contractAuthority =
                InvalidationController.get(mApplicationContext).getContractAuthority();
        if (hasFinishedFirstSync(account)) {
            mSyncContentResolverWrapper.setIsSyncable(account, contractAuthority, 1);
        }
        // Disable the syncability of Chrome for all other accounts
        Account[] googleAccounts = AccountManagerHelper.get(mApplicationContext).
                getGoogleAccounts();
        for (Account accountToSetNotSyncable : googleAccounts) {
            if (!accountToSetNotSyncable.equals(account) &&
                    mSyncContentResolverWrapper.getIsSyncable(
                            accountToSetNotSyncable, contractAuthority) > 0) {
                mSyncContentResolverWrapper.setIsSyncable(accountToSetNotSyncable,
                        contractAuthority, 0);
            }
        }
    }

    /**
     * Returns whether the given account has ever been synced.
     */
    boolean hasFinishedFirstSync(Account account) {
        String contractAuthority =
                InvalidationController.get(mApplicationContext).getContractAuthority();
        return mSyncContentResolverWrapper.getIsSyncable(account, contractAuthority) <= 0;
    }

    /**
     * Helper class to be used by observers whenever sync settings change.
     *
     * To register the observer, call SyncStatusHelper.registerObserver(...).
     */
    public static abstract class SyncSettingsChangedObserver implements SyncStatusObserver {

        @Override
        public void onStatusChanged(int which) {
            if (ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS == which) {
                syncSettingsChanged();
            }
        }

        protected abstract void syncSettingsChanged();
    }

    /**
     * Returns the default shared preferences.
     */
    private SharedPreferences getPreferences() {
        return PreferenceManager.getDefaultSharedPreferences(mApplicationContext);
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

    /**
     * Adds a Listener.
     * @param listener Listener to add.
     */
    public void addListener(Listener listener) {
        mListeners.addObserver(listener);
    }

    /**
     * Removes a Listener.
     * @param listener Listener to remove from the list.
     */
    public void removeListener(Listener listener) {
        mListeners.removeObserver(listener);
    }
}
