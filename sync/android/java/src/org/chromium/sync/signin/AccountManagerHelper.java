// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.signin;


import android.Manifest;
import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorDescription;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.net.NetworkChangeNotifier;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.regex.Pattern;

/**
 * AccountManagerHelper wraps our access of AccountManager in Android.
 *
 * Use the AccountManagerHelper.get(someContext) to instantiate it
 */
public class AccountManagerHelper {
    private static final String TAG = "Sync_Signin";

    private static final Pattern AT_SYMBOL = Pattern.compile("@");

    private static final String GMAIL_COM = "gmail.com";

    private static final String GOOGLEMAIL_COM = "googlemail.com";

    public static final String GOOGLE_ACCOUNT_TYPE = "com.google";

    /**
     * An account feature (corresponding to a Gaia service flag) that specifies whether the account
     * is a child account.
     */
    @VisibleForTesting public static final String FEATURE_IS_CHILD_ACCOUNT_KEY = "service_uca";

    private static final Object sLock = new Object();

    private static final int MAX_TRIES = 3;

    private static AccountManagerHelper sAccountManagerHelper;

    private final AccountManagerDelegate mAccountManager;

    private Context mApplicationContext;

    /**
     * A simple callback for getAuthToken.
     */
    public interface GetAuthTokenCallback {
        /**
         * Invoked on the UI thread if a token is provided by the AccountManager.
         *
         * @param token Auth token, guaranteed not to be null.
         */
        void tokenAvailable(String token);

        /**
         * Invoked on the UI thread if no token is available.
         *
         * @param isTransientError Indicates if the error is transient (network timeout or
         * unavailable, etc) or persistent (bad credentials, permission denied, etc).
         */
        void tokenUnavailable(boolean isTransientError);
    }

    /**
     * @param context the Android context
     * @param accountManager the account manager to use as a backend service
     */
    private AccountManagerHelper(Context context, AccountManagerDelegate accountManager) {
        mApplicationContext = context.getApplicationContext();
        mAccountManager = accountManager;
    }

    /**
     * Initialize AccountManagerHelper with a custom AccountManagerDelegate.
     * Ensures that the singleton AccountManagerHelper hasn't been created yet.
     * This can be overriden in tests using the overrideAccountManagerHelperForTests method.
     *
     * @param context the applicationContext is retrieved from the context used as an argument.
     * @param delegate the custom AccountManagerDelegate to use.
     */
    public static void initializeAccountManagerHelper(
            Context context, AccountManagerDelegate delegate) {
        synchronized (sLock) {
            assert sAccountManagerHelper == null;
            sAccountManagerHelper = new AccountManagerHelper(context, delegate);
        }
    }

    /**
     * A getter method for AccountManagerHelper singleton which also initializes it if not wasn't
     * already initialized.
     *
     * @param context the applicationContext is retrieved from the context used as an argument.
     * @return a singleton instance of the AccountManagerHelper
     */
    public static AccountManagerHelper get(Context context) {
        synchronized (sLock) {
            if (sAccountManagerHelper == null) {
                sAccountManagerHelper = new AccountManagerHelper(
                        context, new SystemAccountManagerDelegate(context));
            }
        }
        return sAccountManagerHelper;
    }

    /**
     * Override AccountManagerHelper with a custom AccountManagerDelegate in tests.
     * Unlike initializeAccountManagerHelper, this will override the existing instance of
     * AccountManagerHelper if any. Only for use in Tests.
     *
     * @param context the applicationContext is retrieved from the context used as an argument.
     * @param delegate the custom AccountManagerDelegate to use.
     */
    @VisibleForTesting
    public static void overrideAccountManagerHelperForTests(
            Context context, AccountManagerDelegate delegate) {
        synchronized (sLock) {
            sAccountManagerHelper = new AccountManagerHelper(context, delegate);
        }
    }

    /**
     * Creates an Account object for the given name.
     */
    public static Account createAccountFromName(String name) {
        return new Account(name, GOOGLE_ACCOUNT_TYPE);
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public List<String> getGoogleAccountNames() {
        List<String> accountNames = new ArrayList<String>();
        for (Account account : getGoogleAccounts()) {
            accountNames.add(account.name);
        }
        return accountNames;
    }

    /**
     * Retrieves a list of the Google account names on the device asynchronously.
     */
    public void getGoogleAccountNames(final Callback<List<String>> callback) {
        getGoogleAccounts(new Callback<Account[]>() {
            @Override
            public void onResult(Account[] accounts) {
                List<String> accountNames = new ArrayList<String>();
                for (Account account : accounts) {
                    accountNames.add(account.name);
                }
                callback.onResult(accountNames);
            }
        });
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public Account[] getGoogleAccounts() {
        return mAccountManager.getAccountsByType(GOOGLE_ACCOUNT_TYPE);
    }

    /**
     * Retrieves all Google accounts on the device asynchronously.
     */
    public void getGoogleAccounts(Callback<Account[]> callback) {
        mAccountManager.getAccountsByType(GOOGLE_ACCOUNT_TYPE, callback);
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public boolean hasGoogleAccounts() {
        return getGoogleAccounts().length > 0;
    }

    /**
     * Asynchronously determine whether any Google accounts have been added.
     */
    public void hasGoogleAccounts(final Callback<Boolean> callback) {
        getGoogleAccounts(new Callback<Account[]>() {
            @Override
            public void onResult(Account[] accounts) {
                callback.onResult(accounts.length > 0);
            }
        });
    }

    private String canonicalizeName(String name) {
        String[] parts = AT_SYMBOL.split(name);
        if (parts.length != 2) return name;

        if (GOOGLEMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[1] = GMAIL_COM;
        }
        if (GMAIL_COM.equalsIgnoreCase(parts[1])) {
            parts[0] = parts[0].replace(".", "");
        }
        return (parts[0] + "@" + parts[1]).toLowerCase(Locale.US);
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public Account getAccountFromName(String accountName) {
        String canonicalName = canonicalizeName(accountName);
        Account[] accounts = getGoogleAccounts();
        for (Account account : accounts) {
            if (canonicalizeName(account.name).equals(canonicalName)) {
                return account;
            }
        }
        return null;
    }

    /**
     * Asynchronously returns the account if it exists; null otherwise.
     */
    public void getAccountFromName(String accountName, final Callback<Account> callback) {
        final String canonicalName = canonicalizeName(accountName);
        getGoogleAccounts(new Callback<Account[]>() {
            @Override
            public void onResult(Account[] accounts) {
                Account accountForName = null;
                for (Account account : accounts) {
                    if (canonicalizeName(account.name).equals(canonicalName)) {
                        accountForName = account;
                        break;
                    }
                }
                callback.onResult(accountForName);
            }
        });
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public boolean hasAccountForName(String accountName) {
        return getAccountFromName(accountName) != null;
    }

    /**
     * Asynchronously returns whether an account exists with the given name.
     */
    public void hasAccountForName(String accountName, final Callback<Boolean> callback) {
        getAccountFromName(accountName, new Callback<Account>() {
            @Override
            public void onResult(Account account) {
                callback.onResult(account != null);
            }
        });
    }

    /**
     * This method is deprecated; please use the asynchronous version below instead.
     *
     * See http://crbug.com/517697 for details.
     */
    public boolean hasGoogleAccountAuthenticator() {
        AuthenticatorDescription[] descs = mAccountManager.getAuthenticatorTypes();
        for (AuthenticatorDescription desc : descs) {
            if (GOOGLE_ACCOUNT_TYPE.equals(desc.type)) return true;
        }
        return false;
    }

    /**
     * Gets the auth token and returns the response asynchronously.
     * This should be called when we have a foreground activity that needs an auth token.
     * If encountered an IO error, it will attempt to retry when the network is back.
     *
     * - Assumes that the account is a valid account.
     */
    public void getAuthToken(Account account, String authTokenType, GetAuthTokenCallback callback) {
        AtomicInteger numTries = new AtomicInteger(0);
        AtomicBoolean isTransientError = new AtomicBoolean(false);
        getAuthTokenAsynchronously(
                account, authTokenType, callback, numTries, isTransientError, null);
    }

    private class ConnectionRetry implements NetworkChangeNotifier.ConnectionTypeObserver {
        private final Account mAccount;
        private final String mAuthTokenType;
        private final GetAuthTokenCallback mCallback;
        private final AtomicInteger mNumTries;
        private final AtomicBoolean mIsTransientError;

        ConnectionRetry(Account account, String authTokenType, GetAuthTokenCallback callback,
                AtomicInteger numTries, AtomicBoolean isTransientError) {
            mAccount = account;
            mAuthTokenType = authTokenType;
            mCallback = callback;
            mNumTries = numTries;
            mIsTransientError = isTransientError;
        }

        @Override
        public void onConnectionTypeChanged(int connectionType) {
            assert mNumTries.get() <= MAX_TRIES;
            if (mNumTries.get() == MAX_TRIES) {
                NetworkChangeNotifier.removeConnectionTypeObserver(this);
                return;
            }
            if (NetworkChangeNotifier.isOnline()) {
                NetworkChangeNotifier.removeConnectionTypeObserver(this);
                getAuthTokenAsynchronously(
                        mAccount, mAuthTokenType, mCallback, mNumTries, mIsTransientError, this);
            }
        }
    }

    private boolean hasUseCredentialsPermission() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
                || mApplicationContext.checkPermission("android.permission.USE_CREDENTIALS",
                Process.myPid(), Process.myUid()) == PackageManager.PERMISSION_GRANTED;
    }

    public boolean hasGetAccountsPermission() {
        return mApplicationContext.checkPermission(Manifest.permission.GET_ACCOUNTS,
                Process.myPid(), Process.myUid()) == PackageManager.PERMISSION_GRANTED;
    }

    // Gets the auth token synchronously
    private String getAuthTokenInner(AccountManagerFuture<Bundle> future,
            AtomicBoolean isTransientError) {
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
            isTransientError.set(true);
        }
        return null;
    }

    private void getAuthTokenAsynchronously(final Account account, final String authTokenType,
            final GetAuthTokenCallback callback, final AtomicInteger numTries,
            final AtomicBoolean isTransientError, final ConnectionRetry retry) {
        // Return null token for no USE_CREDENTIALS permission.
        if (!hasUseCredentialsPermission()) {
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    callback.tokenUnavailable(false);
                }
            });
            return;
        }
        final AccountManagerFuture<Bundle> future = mAccountManager.getAuthToken(
                account, authTokenType, true, null, null);
        isTransientError.set(false);

        new AsyncTask<Void, Void, String>() {
            @Override
            public String doInBackground(Void... params) {
                return getAuthTokenInner(future, isTransientError);
            }
            @Override
            public void onPostExecute(String authToken) {
                onGotAuthTokenResult(account, authTokenType, authToken, callback, numTries,
                        isTransientError, retry);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private void onGotAuthTokenResult(Account account, String authTokenType, String authToken,
            GetAuthTokenCallback callback, AtomicInteger numTries, AtomicBoolean isTransientError,
            ConnectionRetry retry) {
        if (authToken != null) {
            callback.tokenAvailable(authToken);
            return;
        } else if (!isTransientError.get()
                || numTries.incrementAndGet() == MAX_TRIES
                || !NetworkChangeNotifier.isInitialized()) {
            callback.tokenUnavailable(isTransientError.get());
            return;
        }
        if (retry == null) {
            ConnectionRetry newRetry = new ConnectionRetry(account, authTokenType, callback,
                    numTries, isTransientError);
            NetworkChangeNotifier.addConnectionTypeObserver(newRetry);
        } else {
            NetworkChangeNotifier.addConnectionTypeObserver(retry);
        }
    }

    /**
     * Invalidates the old token (if non-null/non-empty) and asynchronously generates a new one.
     *
     * - Assumes that the account is a valid account.
     */
    public void getNewAuthToken(Account account, String authToken, String authTokenType,
            GetAuthTokenCallback callback) {
        invalidateAuthToken(authToken);
        AtomicInteger numTries = new AtomicInteger(0);
        AtomicBoolean isTransientError = new AtomicBoolean(false);
        getAuthTokenAsynchronously(
                account, authTokenType, callback, numTries, isTransientError, null);
    }

    /**
     * Removes an auth token from the AccountManager's cache.
     */
    public void invalidateAuthToken(String authToken) {
        // Cancel operation for no USE_CREDENTIALS permission.
        if (!hasUseCredentialsPermission()) {
            return;
        }
        if (authToken != null && !authToken.isEmpty()) {
            mAccountManager.invalidateAuthToken(GOOGLE_ACCOUNT_TYPE, authToken);
        }
    }

    public void checkChildAccount(Account account, Callback<Boolean> callback) {
        String[] features = {FEATURE_IS_CHILD_ACCOUNT_KEY};
        mAccountManager.hasFeatures(account, features, callback);
    }
}
