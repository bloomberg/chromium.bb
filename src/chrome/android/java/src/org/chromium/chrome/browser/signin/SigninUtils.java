// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;

import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;

/**
 * Helper functions for sign-in and accounts.
 */
public class SigninUtils {
    private static final String ACCOUNT_SETTINGS_ACTION = "android.settings.ACCOUNT_SYNC_SETTINGS";
    private static final String ACCOUNT_SETTINGS_ACCOUNT_KEY = "account";

    private SigninUtils() {}

    /**
     * Opens a Settings page to configure settings for a single account.
     * @param context Context to use when starting the Activity.
     * @param account The account for which the Settings page should be opened.
     * @return Whether or not Android accepted the Intent.
     */
    public static boolean openSettingsForAccount(Context context, Account account) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // ACCOUNT_SETTINGS_ACTION no longer works on Android O+, always open all accounts page.
            return openSettingsForAllAccounts(context);
        }
        Intent intent = new Intent(ACCOUNT_SETTINGS_ACTION);
        intent.putExtra(ACCOUNT_SETTINGS_ACCOUNT_KEY, account);
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return IntentUtils.safeStartActivity(context, intent);
    }

    /**
     * Opens a Settings page with all accounts on the device.
     * @param context Context to use when starting the Activity.
     * @return Whether or not Android accepted the Intent.
     */
    public static boolean openSettingsForAllAccounts(Context context) {
        Intent intent = new Intent(Settings.ACTION_SYNC_SETTINGS);
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return IntentUtils.safeStartActivity(context, intent);
    }

    @CalledByNative
    private static void openAccountManagementScreen(WindowAndroid windowAndroid,
            @GAIAServiceType int gaiaServiceType, @Nullable String email) {
        ThreadUtils.assertOnUiThread();
        AccountManagementFragment.openAccountManagementScreen(gaiaServiceType);
    }

    /**
     * Launches the {@link SigninActivity} if signin is allowed.
     * @param accessPoint {@link SigninAccessPoint} for starting sign-in flow.
     * @return a boolean indicating if the SigninActivity is launched.
     */
    public static boolean startSigninActivityIfAllowed(
            Context context, @SigninAccessPoint int accessPoint) {
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager();
        if (signinManager.isSignInAllowed()) {
            SigninActivityLauncher.get().launchActivity(context, accessPoint);
            return true;
        }
        if (signinManager.isSigninDisabledByPolicy()) {
            ManagedPreferencesUtils.showManagedByAdministratorToast(context);
        }
        return false;
    }

    /**
     * Log a UMA event for a given metric and a signin type.
     * @param metric One of ProfileAccountManagementMetrics constants.
     * @param gaiaServiceType A signin::GAIAServiceType.
     */
    public static void logEvent(int metric, int gaiaServiceType) {
        SigninUtilsJni.get().logEvent(metric, gaiaServiceType);
    }

    @NativeMethods
    interface Natives {
        void logEvent(int metric, int gaiaServiceType);
    }
}
