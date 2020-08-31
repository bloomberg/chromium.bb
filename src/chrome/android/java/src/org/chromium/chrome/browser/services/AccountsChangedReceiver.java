// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services;

import android.accounts.AccountManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninHelper;
import org.chromium.chrome.browser.signin.SigninPreferencesManager;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * This receiver is notified when accounts are added, accounts are removed, or
 * an account's credentials (saved password, etc) are changed.
 * All public methods must be called from the UI thread.
 */
public class AccountsChangedReceiver extends BroadcastReceiver {
    private static final String TAG = "AccountsChangedRx";

    @Override
    public void onReceive(Context context, final Intent intent) {
        if (!AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION.equals(intent.getAction())) return;

        boolean isChromeVisible = ApplicationStatus.hasVisibleActivities();
        if (isChromeVisible) {
            startBrowserIfNeededAndValidateAccounts();
        } else {
            // Notify SigninHelper of changed accounts (via shared prefs).
            SigninPreferencesManager.getInstance().markAccountsChangedPref();
        }
    }

    private static void startBrowserIfNeededAndValidateAccounts() {
        BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
                    // TODO(bsazonov): Check whether invalidateAccountSeedStatus is needed here.
                    IdentityServicesProvider.get()
                            .getAccountTrackerService()
                            .invalidateAccountSeedStatus(false /* don't refresh right now */);
                    SigninHelper.get().validateAccountSettings(true);
                });
            }

            @Override
            public void onStartupFailure(Exception failureCause) {
                // Startup failed. So notify SigninHelper of changed accounts via
                // shared prefs.
                SigninPreferencesManager.getInstance().markAccountsChangedPref();
            }
        };
        ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);
        ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
    }
}
