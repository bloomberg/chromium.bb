// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorDescription;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.content.Context;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.StrictMode;
import android.os.SystemClock;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;

import java.io.IOException;
import java.util.concurrent.TimeUnit;

/**
 * Default implementation of {@link AccountManagerDelegate} which delegates all calls to the
 * Android account manager.
 */
public class SystemAccountManagerDelegate implements AccountManagerDelegate {

    private final AccountManager mAccountManager;
    private final Context mApplicationContext;
    private static final String TAG = "Auth";

    public SystemAccountManagerDelegate(Context context) {
        mApplicationContext = context.getApplicationContext();
        mAccountManager = AccountManager.get(context.getApplicationContext());
    }

    @Override
    public Account[] getAccountsByType(String type) {
        if (!AccountManagerHelper.get(mApplicationContext).hasGetAccountsPermission()) {
            return new Account[]{};
        }
        long now = SystemClock.elapsedRealtime();
        Account[] accounts = mAccountManager.getAccountsByType(type);
        long elapsed = SystemClock.elapsedRealtime() - now;
        recordElapsedTimeHistogram("Signin.AndroidGetAccountsTime_AccountManager", elapsed);
        return accounts;
    }

    @Override
    public void getAccountsByType(final String type, final Callback<Account[]> callback) {
        new AsyncTask<Void, Void, Account[]>() {
            @Override
            protected Account[] doInBackground(Void... params) {
                return getAccountsByType(type);
            }

            @Override
            protected void onPostExecute(Account[] accounts) {
                callback.onResult(accounts);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public AccountManagerFuture<Bundle> getAuthToken(Account account, String authTokenType,
            boolean notifyAuthFailure, AccountManagerCallback<Bundle> callback, Handler handler) {
        return mAccountManager.getAuthToken(account, authTokenType, null, notifyAuthFailure,
                callback, handler);
    }

    @Override
    public void invalidateAuthToken(String accountType, String authToken) {
        // Temporarily allowing disk access while fixing. TODO: http://crbug.com/535320
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        StrictMode.allowThreadDiskReads();
        try {
            mAccountManager.invalidateAuthToken(accountType, authToken);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public AuthenticatorDescription[] getAuthenticatorTypes() {
        return mAccountManager.getAuthenticatorTypes();
    }

    @Override
    public void hasFeatures(Account account, String[] features, final Callback<Boolean> callback) {
        if (!AccountManagerHelper.get(mApplicationContext).hasGetAccountsPermission()) {
            ThreadUtils.postOnUiThread(new Runnable() {
                @Override
                public void run() {
                    callback.onResult(false);
                }
            });
            return;
        }
        mAccountManager.hasFeatures(account, features, new AccountManagerCallback<Boolean>() {
            @Override
            public void run(AccountManagerFuture<Boolean> future) {
                assert future.isDone();
                boolean hasFeatures = false;
                try {
                    hasFeatures = future.getResult();
                } catch (AuthenticatorException | IOException e) {
                    Log.e(TAG, "Error while checking features: ", e);
                } catch (OperationCanceledException e) {
                    Log.e(TAG, "Checking features was cancelled. This should not happen.");
                }
                callback.onResult(hasFeatures);
            }
        }, null /* handler */);
    }

    /**
     * Records a histogram value for how long time an action has taken using
     * {@link RecordHistogram#recordTimesHistogram(String, long, TimeUnit))} iff the browser
     * process has been initialized.
     *
     * @param histogramName the name of the histogram.
     * @param elapsedMs the elapsed time in milliseconds.
     */
    protected static void recordElapsedTimeHistogram(String histogramName, long elapsedMs) {
        if (!LibraryLoader.isInitialized()) return;
        RecordHistogram.recordTimesHistogram(histogramName, elapsedMs, TimeUnit.MILLISECONDS);
    }
}
