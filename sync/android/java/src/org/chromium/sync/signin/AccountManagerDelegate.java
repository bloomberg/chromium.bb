// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.signin;

import android.accounts.Account;
import android.accounts.AuthenticatorDescription;

import org.chromium.base.Callback;

/**
 * Wrapper around the Android account manager, to facilitate dependency injection during testing.
 */
public interface AccountManagerDelegate {
    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    Account[] getAccountsByType(String type);

    /**
     * Get all the accounts for a given {@code type}.
     */
    void getAccountsByType(String type, Callback<Account[]> callback);

    /**
     * @param account The {@link Account} for which the auth token is requested.
     * @param authTokenScope The scope of the authToken being requested.
     * @return The auth token fetched from the authenticator.
     * The authenticator can throw an {@link AuthException} to indicate a failure in fetching the
     * auth token perhaps due to a transient error or when user intervention is required (like
     * confirming the credentials) which is expressed as an {@link Intent} to the handler.
     * This should only be called on a background thread.
     */
    String getAuthToken(Account account, String authTokenScope) throws AuthException;

    /**
     * Invalidate the {@code authToken} associated with account type {@code accountType}.
     */
    void invalidateAuthToken(String accountType, String authToken);

    /**
     * Get all the available authenticator types.
     */
    AuthenticatorDescription[] getAuthenticatorTypes();

    /**
     * Check whether the {@code account} has all the features listed in {@code features}.
     */
    void hasFeatures(Account account, String[] features, Callback<Boolean> callback);
}
