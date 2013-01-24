// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.signin;


import com.google.common.annotations.VisibleForTesting;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Log;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * AccountManagerHelper wraps our access of AccountManager in Android.
 *
 * Use the AccountManagerHelper.get(someContext) to instantiate it
 */
public class AccountManagerHelper {

    private static final String TAG = "AccountManagerHelper";

    private static final String GOOGLE_ACCOUNT_TYPE = "com.google";

    private static final Object lock = new Object();

    private static AccountManagerHelper sAccountManagerHelper;

    private final AccountManagerDelegate mAccountManager;

    private Context mApplicationContext;

    public interface GetAuthTokenCallback {
        /**
         * Invoked on the UI thread once a token has been provided by the AccountManager.
         * @param token Auth token, or null if no token is available (bad credentials,
         *      permission denied, etc).
         */
        void tokenAvailable(String token);
    }

    /**
     * @param context the Android context
     * @param accountManager the account manager to use as a backend service
     */
    private AccountManagerHelper(Context context,
                                 AccountManagerDelegate accountManager) {
        mApplicationContext = context.getApplicationContext();
        mAccountManager = accountManager;
    }

    /**
     * A factory method for the AccountManagerHelper.
     *
     * It is possible to override the AccountManager to use in tests for the instance of the
     * AccountManagerHelper by calling overrideAccountManagerHelperForTests(...) with
     * your MockAccountManager.
     *
     * @param context the applicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the AccountManagerHelper
     */
    public static AccountManagerHelper get(Context context) {
        synchronized (lock) {
            if (sAccountManagerHelper == null) {
                sAccountManagerHelper = new AccountManagerHelper(context,
                        new SystemAccountManagerDelegate(context));
            }
        }
        return sAccountManagerHelper;
    }

    @VisibleForTesting
    public static void overrideAccountManagerHelperForTests(Context context,
            AccountManagerDelegate accountManager) {
        synchronized (lock) {
            sAccountManagerHelper = new AccountManagerHelper(context, accountManager);
        }
    }

    /**
     * Creates an Account object for the given name.
     */
    public static Account createAccountFromName(String name) {
        return new Account(name, GOOGLE_ACCOUNT_TYPE);
    }

    public List<String> getGoogleAccountNames() {
        List<String> accountNames = new ArrayList<String>();
        Account[] accounts = mAccountManager.getAccountsByType(GOOGLE_ACCOUNT_TYPE);
        for (Account account : accounts) {
            accountNames.add(account.name);
        }
        return accountNames;
    }

    public Account[] getGoogleAccounts() {
        return mAccountManager.getAccountsByType(GOOGLE_ACCOUNT_TYPE);
    }

    public boolean hasGoogleAccounts() {
        return getGoogleAccounts().length > 0;
    }

    /**
     * Returns the account if it exists, null otherwise.
     */
    public Account getAccountFromName(String accountName) {
        Account[] accounts = mAccountManager.getAccountsByType(GOOGLE_ACCOUNT_TYPE);
        for (Account account : accounts) {
            if (account.name.equals(accountName)) {
                return account;
            }
        }
        return null;
    }

    /**
     * Gets the auth token synchronously.
     *
     * - Assumes that the account is a valid account.
     * - Should not be called on the main thread.
     */
    public String getAuthTokenFromBackground(Account account, String authTokenType) {
        try {
            AccountManagerFuture<Bundle> future = mAccountManager.getAuthToken(account,
                    authTokenType, false, null, null);
            Bundle result = future.getResult();
            if (result == null) {
                Log.w(TAG, "Auth token - getAuthToken returned null");
                return null;
            }
            if (result.containsKey(AccountManager.KEY_INTENT)) {
                Log.d(TAG, "Starting intent to get auth credentials");
                // Need to start intent to get credentials
                Intent intent = result.getParcelable(AccountManager.KEY_INTENT);
                int flags = intent.getFlags();
                flags |= Intent.FLAG_ACTIVITY_NEW_TASK;
                intent.setFlags(flags);
                mApplicationContext.startActivity(intent);
                return null;
            }
            return result.getString(AccountManager.KEY_AUTHTOKEN);
        } catch (OperationCanceledException e) {
            Log.w(TAG, "Auth token - operation cancelled", e);
        } catch (AuthenticatorException e) {
            Log.w(TAG, "Auth token - authenticator exception", e);
        } catch (IOException e) {
            Log.w(TAG, "Auth token - IO exception", e);
        }
        return null;
    }

    /**
     * Gets the auth token and returns the response asynchronously on the UI thread.
     * This should be called when we have a foreground activity that needs an auth token.
     *
     * - Assumes that the account is a valid account.
     */
   public void getAuthTokenFromForeground(Activity activity, Account account,
           String authTokenType, final GetAuthTokenCallback callback) {
       final AccountManagerFuture<Bundle> future = mAccountManager.getAuthToken(account,
               authTokenType, null, activity, null, null);
       new AsyncTask<Void, Void, String>() {
           @Override
           public String doInBackground(Void... params) {
               try {
                   Bundle result = future.getResult();
                   if (result != null) {
                       return result.getString(AccountManager.KEY_AUTHTOKEN);
                   } else {
                       Log.w(TAG, "Auth token - getAuthToken returned null");
                   }
               } catch (OperationCanceledException e) {
                   Log.w(TAG, "Auth token - operation cancelled", e);
               } catch (AuthenticatorException e) {
                   Log.w(TAG, "Auth token - authenticator exception", e);
               } catch (IOException e) {
                   Log.w(TAG, "Auth token - IO exception", e);
               }
               return null;
           }
           @Override
           public void onPostExecute(String authToken) {
               callback.tokenAvailable(authToken);
           }
       }.execute();
   }

    /**
     * Invalidates the old token (if non-null/non-empty) and synchronously generates a new one.
     * Also notifies the user (via status bar) if any user action is required. The method will
     * return null if any user action is required to generate the new token.
     *
     * - Assumes that the account is a valid account.
     * - Should not be called on the main thread.
     */
    public String getNewAuthToken(Account account, String authToken, String authTokenType) {
        if (authToken != null && !authToken.isEmpty()) {
            mAccountManager.invalidateAuthToken(GOOGLE_ACCOUNT_TYPE, authToken);
        }

        try {
            return mAccountManager.blockingGetAuthToken(account, authTokenType, true);
        } catch (OperationCanceledException e) {
            Log.w(TAG, "Auth token - operation cancelled", e);
        } catch (AuthenticatorException e) {
            Log.w(TAG, "Auth token - authenticator exception", e);
        } catch (IOException e) {
            Log.w(TAG, "Auth token - IO exception", e);
        }
        return null;
    }
}
