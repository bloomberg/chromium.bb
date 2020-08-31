// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Bundle;
import android.os.UserManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;

/** Provides first run related utility functions. */
public class FirstRunUtils {
    private static Boolean sHasGoogleAccountAuthenticator;

    /**
     * Synchronizes first run native and Java preferences.
     * Must be called after native initialization.
     */
    public static void cacheFirstRunPrefs() {
        SharedPreferencesManager javaPrefs = SharedPreferencesManager.getInstance();
        // Set both Java and native prefs if any of the three indicators indicate ToS has been
        // accepted. This needed because:
        //   - Old versions only set native pref, so this syncs Java pref.
        //   - Backup & restore does not restore native pref, so this needs to update it.
        //   - checkAnyUserHasSeenToS() may be true which needs to sync its state to the prefs.
        boolean javaPrefValue =
                javaPrefs.readBoolean(ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, false);
        boolean nativePrefValue = isFirstRunEulaAccepted();
        boolean userHasSeenTos =
                ToSAckedReceiver.checkAnyUserHasSeenToS();
        boolean isFirstRunComplete = FirstRunStatus.getFirstRunFlowComplete();
        if (javaPrefValue || nativePrefValue || userHasSeenTos || isFirstRunComplete) {
            if (!javaPrefValue) {
                javaPrefs.writeBoolean(ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, true);
            }
            if (!nativePrefValue) {
                setEulaAccepted();
            }
        }
    }

    /**
     * @return Whether the user has accepted Chrome Terms of Service.
     */
    public static boolean didAcceptTermsOfService() {
        // Note: Does not check FirstRunUtils.isFirstRunEulaAccepted() because this may be called
        // before native is initialized.
        return SharedPreferencesManager.getInstance().readBoolean(
                       ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, false)
                || ToSAckedReceiver.checkAnyUserHasSeenToS();
    }

    /**
     * Sets the EULA/Terms of Services state as "ACCEPTED".
     * @param allowCrashUpload True if the user allows to upload crash dumps and collect stats.
     */
    public static void acceptTermsOfService(boolean allowCrashUpload) {
        UmaSessionStats.changeMetricsReportingConsent(allowCrashUpload);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_CACHED_TOS_ACCEPTED, true);
        setEulaAccepted();
    }

    /**
     * Determines whether or not the user has a Google account (so we can sync) or can add one.
     * @return Whether or not sync is allowed on this device.
     */
    static boolean canAllowSync() {
        return (hasGoogleAccountAuthenticator() && hasSyncPermissions()) || hasGoogleAccounts();
    }

    @VisibleForTesting
    static boolean hasGoogleAccountAuthenticator() {
        if (sHasGoogleAccountAuthenticator == null) {
            AccountManagerFacade accountHelper = AccountManagerFacadeProvider.getInstance();
            sHasGoogleAccountAuthenticator = accountHelper.hasGoogleAccountAuthenticator();
        }
        return sHasGoogleAccountAuthenticator;
    }

    @VisibleForTesting
    static boolean hasGoogleAccounts() {
        return !AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts().isEmpty();
    }

    @SuppressLint("InlinedApi")
    private static boolean hasSyncPermissions() {
        UserManager manager = (UserManager) ContextUtils.getApplicationContext().getSystemService(
                Context.USER_SERVICE);
        Bundle userRestrictions = manager.getUserRestrictions();
        return !userRestrictions.getBoolean(UserManager.DISALLOW_MODIFY_ACCOUNTS, false);
    }

    /**
     * @return Whether EULA has been accepted by the user.
     */
    public static boolean isFirstRunEulaAccepted() {
        return FirstRunUtilsJni.get().getFirstRunEulaAccepted();
    }

    /**
     * Sets the preference that signals when the user has accepted the EULA.
     */
    public static void setEulaAccepted() {
        FirstRunUtilsJni.get().setEulaAccepted();
    }

    @NativeMethods
    public interface Natives {
        boolean getFirstRunEulaAccepted();
        void setEulaAccepted();
    }
}
