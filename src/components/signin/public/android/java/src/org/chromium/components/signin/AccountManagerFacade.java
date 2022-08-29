// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.components.signin.base.AccountCapabilities;

import java.util.List;

/**
 * Interface for {@link AccountManagerFacadeImpl}.
 */
public interface AccountManagerFacade {
    /**
     * Listener for whether the account is a child one.
     */
    interface ChildAccountStatusListener {
        /**
         * The method is called when the status of the account (whether it is a child one) is ready.
         *
         * @param isChild If account is a child account.
         * @param childAccount The child account if isChild != false; null
         *         otherwise.
         *
         * TODO(crbug.com/1258563): consider refactoring this interface to use Promises.
         */
        void onStatusReady(boolean isChild, @Nullable Account childAccount);
    }

    /**
     * Adds an observer to receive accounts change notifications.
     * @param observer the observer to add.
     */
    @MainThread
    void addObserver(AccountsChangeObserver observer);

    /**
     * Removes an observer that was previously added using {@link #addObserver}.
     * @param observer the observer to remove.
     */
    @MainThread
    void removeObserver(AccountsChangeObserver observer);

    /**
     * Retrieves all the accounts on the device.
     * The {@link Promise} will be fulfilled once the accounts cache will be populated.
     * If an error occurs while getting account list, the returned {@link Promise} will wrap an
     * empty array.
     *
     * Since a different {@link Promise} will be returned every time the accounts get updated,
     * this makes it a bad candidate for end users to cache the {@link Promise} locally unless
     * the end users are awaiting the current list of accounts only.
     */
    @MainThread
    Promise<List<Account>> getAccounts();

    /**
     * @return Whether or not there is an account authenticator for Google accounts.
     */
    @AnyThread
    boolean hasGoogleAccountAuthenticator();

    /**
     * Synchronously gets an OAuth2 access token. May return a cached version, use
     * {@link #invalidateAccessToken} to invalidate a token in the cache.
     * @param account The {@link Account} for which the token is requested.
     * @param scope OAuth2 scope for which the requested token should be valid.
     * @return The OAuth2 access token as an AccessTokenData with a string and an expiration time.
     */
    @WorkerThread
    AccessTokenData getAccessToken(Account account, String scope) throws AuthException;

    /**
     * Removes an OAuth2 access token from the cache with retries asynchronously.
     * Uses {@link #getAccessToken} to issue a new token after invalidating the old one.
     * @param accessToken The access token to invalidate.
     */
    @MainThread
    void invalidateAccessToken(String accessToken);

    /**
     * Checks the child account status of the given account.
     *
     * @param account The account to check the child account status.
     * @param listener The listener is called when the status of the account
     *                 (whether it is a child one) is ready.
     */
    @MainThread
    void checkChildAccountStatus(Account account, ChildAccountStatusListener listener);

    /**
     * @param account The account used to look up capabilities.
     * @return account capabilities for the given account.
     */
    @MainThread
    Promise<AccountCapabilities> getAccountCapabilities(Account account);

    /**
     * Creates an intent that will ask the user to add a new account to the device. See
     * {@link AccountManager#addAccount} for details.
     * @param callback The callback to get the created intent. Will be invoked on the main
     *         thread. If there is an issue while creating the intent, callback will receive
     *         null.
     */
    @AnyThread
    void createAddAccountIntent(Callback<Intent> callback);

    /**
     * Asks the user to enter a new password for an account, updating the saved credentials for
     * the account.
     */
    @MainThread
    void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback);

    /**
     * Returns the Gaia id for the account associated with the given email address.
     * If an account with the given email address is not installed on the device
     * then null is returned.
     *
     * This method will throw IllegalStateException if called on the main thread.
     *
     * @param accountEmail The email address of a Google account.
     */
    @WorkerThread
    @Nullable
    String getAccountGaiaId(String accountEmail);
}
