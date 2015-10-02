// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.signin;

import android.accounts.Account;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorDescription;
import android.os.Bundle;
import android.os.Handler;

/**
 * Wrapper around the Android account manager, to facilitate dependency injection during testing.
 */
public interface AccountManagerDelegate {
    /**
     * A callback class that can be used to allow asynchronous methods.
     */
    interface Callback<T> {
        void gotResult(T value);
    }

    /**
     * Use the asynchronous getAccountsByType(String, Callback<Account[]>) instead.
     */
    @Deprecated
    Account[] getAccountsByType(String type);

    void getAccountsByType(String type, Callback<Account[]> callback);

    AccountManagerFuture<Bundle> getAuthToken(Account account, String authTokenType,
            boolean notifyAuthFailure, AccountManagerCallback<Bundle> callback, Handler handler);

    void invalidateAuthToken(String accountType, String authToken);

    AuthenticatorDescription[] getAuthenticatorTypes();

    void hasFeatures(Account account, String[] features, Callback<Boolean> callback);
}
