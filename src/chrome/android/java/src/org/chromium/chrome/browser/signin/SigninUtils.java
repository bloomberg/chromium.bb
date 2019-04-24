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

import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Helper functions for sign-in and accounts.
 */
public class SigninUtils {
    private static final String ACCOUNT_SETTINGS_ACTION = "android.settings.ACCOUNT_SYNC_SETTINGS";
    private static final String ACCOUNT_SETTINGS_ACCOUNT_KEY = "account";

    private SigninUtils() {}

    /**
     * Opens a Settings page to configure settings for a single account.
     * Note: on Android O+, this method is identical to {@link #openSettingsForAllAccounts}.
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
}
