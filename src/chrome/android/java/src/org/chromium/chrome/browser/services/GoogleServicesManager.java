// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services;

import android.annotation.SuppressLint;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ApplicationStateListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninHelper;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.SigninPreferencesManager;
import org.chromium.chrome.browser.sync.SyncController;
import org.chromium.components.signin.ChromeSigninController;
import org.chromium.components.signin.metrics.SignoutReason;

/**
 * Starts and monitors various sync and Google services related tasks.
 * - add listeners to AccountManager.
 * - sets up the Android status bar notification controller.
 * - start Tango service if sync setup is completed.
 * <p/>
 * It is intended to be an application level object and is not tied to any particulary
 * activity, although re-verifies some settings whe browser is launched.
 * <p/>
 * The object must be created on the main thread.
 * <p/>
 */
public class GoogleServicesManager implements ApplicationStateListener {

    private static final String TAG = "GoogleServicesManager";

    @VisibleForTesting
    public static final String SESSION_TAG_PREFIX = "session_sync";

    @SuppressLint("StaticFieldLeak")
    private static GoogleServicesManager sGoogleServicesManager;

    private final ChromeSigninController mChromeSigninController;

    private final SigninHelper mSigninHelper;

    /**
     * A helper method for retrieving the application-wide GoogleServicesManager.
     * <p/>
     * Can only be accessed on the main thread.
     *
     * @return a singleton instance of the GoogleServicesManager
     */
    public static GoogleServicesManager get() {
        ThreadUtils.assertOnUiThread();
        if (sGoogleServicesManager == null) {
            sGoogleServicesManager = new GoogleServicesManager();
        }
        return sGoogleServicesManager;
    }

    private GoogleServicesManager() {
        try {
            TraceEvent.begin("GoogleServicesManager.GoogleServicesManager");
            ThreadUtils.assertOnUiThread();

            mChromeSigninController = ChromeSigninController.get();
            mSigninHelper = SigninHelper.get();

            // The sign out flow starts by clearing the signed in user in the ChromeSigninController
            // on the Java side, and then performs a sign out on the native side. If there is a
            // crash on the native side then the signin state may get out of sync. Make sure that
            // the native side is signed out if the Java side doesn't have a currently signed in
            // user.
            // TODO(bsazonov): Move this to SigninManager.
            SigninManager signinManager = IdentityServicesProvider.get().getSigninManager();
            if (!mChromeSigninController.isSignedIn()
                    && signinManager.getIdentityManager().hasPrimaryAccount()) {
                Log.w(TAG, "Signed in state got out of sync, forcing native sign out");
                // TODO(https://crbug.com/873116): Pass the correct reason for the signout.
                signinManager.signOut(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);
            }

            // Initialize sync.
            SyncController.get();

            ApplicationStatus.registerApplicationStateListener(this);
        } finally {
            TraceEvent.end("GoogleServicesManager.GoogleServicesManager");
        }
    }

    /**
     * Called once during initialization and then again for every start (warm-start).
     * Responsible for checking if configuration has changed since Chrome was last launched
     * and updates state accordingly.
     */
    public void onMainActivityStart() {
        try {
            TraceEvent.begin("GoogleServicesManager.onMainActivityStart");
            boolean accountsChanged =
                    SigninPreferencesManager.getInstance().checkAndClearAccountsChangedPref();
            mSigninHelper.validateAccountSettings(accountsChanged);
        } finally {
            TraceEvent.end("GoogleServicesManager.onMainActivityStart");
        }
    }

    @Override
    public void onApplicationStateChange(int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            onMainActivityStart();
        }
    }
}
