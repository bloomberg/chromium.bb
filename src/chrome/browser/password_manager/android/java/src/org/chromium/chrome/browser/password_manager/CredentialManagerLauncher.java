// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Interface for the launcher responsible for opening the Credential Manager.
 */
public interface CredentialManagerLauncher {
    /**
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused. They should be kept in sync with the enum values
     * in enums.xml.
     */
    @IntDef({CredentialManagerError.NO_CONTEXT, CredentialManagerError.NO_ACCOUNT_NAME,
            CredentialManagerError.API_ERROR, CredentialManagerError.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CredentialManagerError {
        // There is no application context.
        int NO_CONTEXT = 0;
        // The provided account name is empty
        int NO_ACCOUNT_NAME = 1;
        // Error encountered after calling the API to fetch the launch intent.
        int API_ERROR = 2;
        int COUNT = 3;
    }

    /**
     * Retrieves a pending intent that can be used to launch the credential manager. The intent
     * is to either be used immediately or discarded.
     *
     * @param referrer the place that will launch the credential manager
     * @param accountName the account name that is syncing passwords.
     * @param successCallback callback called with the intent if the retrieving was successful
     * @param failureCallback callback called if the retrieving failed with the encountered error.
     *      The error should be a value from {@link CredentialManagerError}.
     */
    void getCredentialManagerIntentForAccount(@ManagePasswordsReferrer int referrer,
            String accountName, Callback<PendingIntent> successCallback,
            Callback<Integer> failureCallback);

    /**
     * Retrieves a pending intent that can be used to launch the credential manager. The intent
     * is to either be used immediately or discarded.
     *
     * @param referrer the place that will launch the credential manager
     * @param successCallback callback called with the intent if the retrieving was successful
     * @param failureCallback callback called if the retrieving failed with the encountered error
     *      The error should be a value from {@link CredentialManagerError}.
     */
    void getCredentialManagerIntentForLocal(@ManagePasswordsReferrer int referrer,
            Callback<PendingIntent> successCallback, Callback<Integer> failureCallback);
}
