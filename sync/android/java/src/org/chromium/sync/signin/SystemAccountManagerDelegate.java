// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorDescription;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;

/**
 * Default implementation of {@link AccountManagerDelegate} which delegates all calls to the
 * Android account manager.
 */
public class SystemAccountManagerDelegate implements AccountManagerDelegate {

    private final AccountManager mAccountManager;
    private final Context mApplicationContext;

    public SystemAccountManagerDelegate(Context context) {
        mApplicationContext = context.getApplicationContext();
        mAccountManager = AccountManager.get(context.getApplicationContext());
    }

    @Override
    public Account[] getAccountsByType(String type) {
        if (!AccountManagerHelper.get(mApplicationContext).hasGetAccountsPermission()) {
            return new Account[]{};
        }
        return mAccountManager.getAccountsByType(type);
    }

    @Override
    public AccountManagerFuture<Bundle> getAuthToken(Account account, String authTokenType,
            boolean notifyAuthFailure, AccountManagerCallback<Bundle> callback, Handler handler) {
        return mAccountManager.getAuthToken(account, authTokenType, null, notifyAuthFailure,
                callback, handler);
    }

    @Override
    public void invalidateAuthToken(String accountType, String authToken) {
        mAccountManager.invalidateAuthToken(accountType, authToken);
    }

    @Override
    public AuthenticatorDescription[] getAuthenticatorTypes() {
        return mAccountManager.getAuthenticatorTypes();
    }

    @Override
    public AccountManagerFuture<Boolean> hasFeatures(Account account, String[] features,
            AccountManagerCallback<Boolean> callback, Handler handler) {
        return mAccountManager.hasFeatures(account, features, callback, handler);
    }
}
