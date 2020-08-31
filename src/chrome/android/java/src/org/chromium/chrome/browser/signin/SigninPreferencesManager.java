// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.util.Set;

/**
 * SigninPreferencesManager stores the state of SharedPreferences related to account sign-in.
 */
public class SigninPreferencesManager {
    private static final SigninPreferencesManager INSTANCE = new SigninPreferencesManager();

    private final SharedPreferencesManager mManager;

    private SigninPreferencesManager() {
        mManager = SharedPreferencesManager.getInstance();
    }

    /**
     * @return the SignInPromoStore singleton
     */
    public static SigninPreferencesManager getInstance() {
        return INSTANCE;
    }

    /**
     * Sets the {@link ChromePreferenceKeys#SIGNIN_ACCOUNTS_CHANGED} to true.
     */
    public void markAccountsChangedPref() {
        // The process may go away as soon as we return from onReceive but Android makes sure
        // that in-flight disk writes from apply() complete before changing component states.
        mManager.writeBoolean(ChromePreferenceKeys.SIGNIN_ACCOUNTS_CHANGED, true);
    }

    /**
     * @return The new account name of the current user. Null if it wasn't renamed.
     */
    String getNewSignedInAccountName() {
        return mManager.readString(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAMED, null);
    }

    /**
     * Sets the new account name of the current user.
     *
     * @param newName the new name to write
     */
    void setNewSignedInAccountName(@Nullable String newName) {
        mManager.writeString(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAMED, newName);
    }

    /**
     * Clears the new account name of the current user.
     */
    void clearNewSignedInAccountName() {
        setNewSignedInAccountName(null);
    }

    /**
     * Sets the last read index of all the account changed events of the current signed in account.
     *
     * @param newIndex the new index to write
     */
    void setLastAccountChangedEventIndex(int newIndex) {
        mManager.writeInt(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAME_EVENT_INDEX, newIndex);
    }

    /**
     * @return the last read index of all the account changed events of the current signed in
     *         account.
     */
    int getLastAccountChangedEventIndex() {
        return mManager.readInt(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAME_EVENT_INDEX);
    }

    /**
     * Gets the state of {@link ChromePreferenceKeys#SIGNIN_ACCOUNTS_CHANGED} and clears it.
     *
     * @return the state of {@link ChromePreferenceKeys#SIGNIN_ACCOUNTS_CHANGED} before the call.
     */
    public boolean checkAndClearAccountsChangedPref() {
        if (mManager.readBoolean(ChromePreferenceKeys.SIGNIN_ACCOUNTS_CHANGED, false)) {
            // Clear the value in prefs.
            mManager.writeBoolean(ChromePreferenceKeys.SIGNIN_ACCOUNTS_CHANGED, false);
            return true;
        } else {
            return false;
        }
    }

    /**
     * Clears the accounts state-related shared prefs.
     */
    @VisibleForTesting
    public void clearAccountsStateSharedPrefsForTesting() {
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAME_EVENT_INDEX);
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_ACCOUNT_RENAMED);
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_ACCOUNTS_CHANGED);
    }

    /**
     * Returns Chrome major version number when signin promo was last shown, or 0 if version number
     * isn't known.
     */
    int getSigninPromoLastShownVersion() {
        return mManager.readInt(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION);
    }

    /**
     * Sets Chrome major version number when signin promo was last shown.
     */
    void setSigninPromoLastShownVersion(int majorVersion) {
        mManager.writeInt(ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_MAJOR_VERSION, majorVersion);
    }

    /**
     * Returns a set of account names on the device when signin promo was last shown,
     * or null if promo hasn't been shown yet.
     */
    @Nullable
    Set<String> getSigninPromoLastAccountNames() {
        return mManager.readStringSet(
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, null);
    }

    /**
     * Stores a set of account names on the device when signin promo is shown.
     */
    void setSigninPromoLastAccountNames(Set<String> accountNames) {
        mManager.writeStringSet(
                ChromePreferenceKeys.SIGNIN_PROMO_LAST_SHOWN_ACCOUNT_NAMES, accountNames);
    }

    /**
     * Returns timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed; zero otherwise.
     * @return the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public long getNewTabPageSigninPromoSuppressionPeriodStart() {
        return mManager.readLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START);
    }

    /**
     * Sets timestamp of the suppression period start if signin promos in the New Tab Page are
     * temporarily suppressed.
     * @param timeMillis the epoch time in milliseconds (see {@link System#currentTimeMillis()}).
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void setNewTabPageSigninPromoSuppressionPeriodStart(long timeMillis) {
        mManager.writeLong(
                ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START, timeMillis);
    }

    /**
     * Removes the stored timestamp of the suppression period start when signin promos in the New
     * Tab Page are no longer suppressed.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void clearNewTabPageSigninPromoSuppressionPeriodStart() {
        mManager.removeKey(ChromePreferenceKeys.SIGNIN_PROMO_NTP_PROMO_SUPPRESSION_PERIOD_START);
    }
}
