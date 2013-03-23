// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.test.util;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;

import org.chromium.sync.signin.AccountManagerDelegate;
import org.chromium.sync.signin.AccountManagerHelper;

import java.io.IOException;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.Callable;
import java.util.concurrent.CancellationException;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import javax.annotation.Nullable;

/**
 * The MockAccountManager helps out if you want to mock out all calls to the Android AccountManager.
 *
 * You should provide a set of accounts as a constructor argument, or use the more direct approach
 * and provide an array of AccountHolder objects.
 *
 * Currently, this implementation supports adding and removing accounts, handling credentials
 * (including confirming them), and handling of dummy auth tokens.
 *
 * If you want the MockAccountManager to popup an activity for granting/denying access to an
 * authtokentype for a given account, use prepareGrantAppPermission(...).
 *
 * If you want to auto-approve a given authtokentype, use addAccountHolderExplicitly(...) with
 * an AccountHolder you have built with hasBeenAccepted("yourAuthTokenType", true).
 *
 * If you want to auto-approve all auth token types for a given account, use the {@link
 * AccountHolder} builder method alwaysAccept(true).
 */
public class MockAccountManager implements AccountManagerDelegate {

    private static final String TAG = MockAccountManager.class.getSimpleName();

    private static final int WAIT_TIME_FOR_GRANT_BROADCAST_MS = 20000;

    static final String MUTEX_WAIT_ACTION =
            "org.chromium.sync.test.util.MockAccountManager.MUTEX_WAIT_ACTION";

    protected final Context mContext;

    private final Context mTestContext;

    private final Set<AccountHolder> mAccounts;

    private final List<AccountAuthTokenPreparation> mAccountPermissionPreparations;

    private final Handler mMainHandler;

    private final SingleThreadedExecutor mExecutor;

    public MockAccountManager(Context context, Context testContext, Account... accounts) {
        mContext = context;
        // The manifest that is backing testContext needs to provide the
        // MockGrantCredentialsPermissionActivity.
        mTestContext = testContext;
        mMainHandler = new Handler(mContext.getMainLooper());
        mExecutor = new SingleThreadedExecutor();
        mAccounts = new HashSet<AccountHolder>();
        mAccountPermissionPreparations = new LinkedList<AccountAuthTokenPreparation>();
        if (accounts != null) {
            for (Account account : accounts) {
                mAccounts.add(AccountHolder.create().account(account).alwaysAccept(true).build());
            }
        }
    }

    private static class SingleThreadedExecutor extends ThreadPoolExecutor {
        public SingleThreadedExecutor() {
            super(1, 1, 1, TimeUnit.SECONDS, new LinkedBlockingDeque<Runnable>());
        }
    }

    @Override
    public Account[] getAccounts() {
        return getAccountsByType(null);
    }

    @Override
    public Account[] getAccountsByType(@Nullable String type) {
        if(!AccountManagerHelper.GOOGLE_ACCOUNT_TYPE.equals(type)) {
            throw new IllegalArgumentException("Invalid account type: " + type);
        }
        if (mAccounts == null) {
            return new Account[0];
        } else {
            Account[] accounts = new Account[mAccounts.size()];
            int i = 0;
            for (AccountHolder ah : mAccounts) {
                accounts[i++] = ah.getAccount();
            }
            return accounts;
        }
    }

    @Override
    public boolean addAccountExplicitly(Account account, String password, Bundle userdata) {
        AccountHolder accountHolder =
                AccountHolder.create().account(account).password(password).build();
        return addAccountHolderExplicitly(accountHolder);
    }

    public boolean addAccountHolderExplicitly(AccountHolder accountHolder) {
        boolean result = mAccounts.add(accountHolder);
        postAsyncAccountChangedEvent();
        return result;
    }

    @Override
    public AccountManagerFuture<Boolean> removeAccount(Account account,
            AccountManagerCallback<Boolean> callback, Handler handler) {
        mAccounts.remove(getAccountHolder(account));
        postAsyncAccountChangedEvent();
        return runTask(mExecutor,
                new AccountManagerTask<Boolean>(handler, callback, new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        // Removal always successful.
                        return true;
                    }
                }));
    }

    @Override
    public String getPassword(Account account) {
        return getAccountHolder(account).getPassword();
    }

    @Override
    public void setPassword(Account account, String password) {
        mAccounts.add(getAccountHolder(account).withPassword(password));
    }

    @Override
    public void clearPassword(Account account) {
        setPassword(account, null);
    }

    @Override
    public AccountManagerFuture<Bundle> confirmCredentials(Account account, Bundle bundle,
            Activity activity, AccountManagerCallback<Bundle> callback, Handler handler) {
        String password = bundle.getString(AccountManager.KEY_PASSWORD);
        if (password == null) {
            throw new IllegalArgumentException("Password is null");
        }
        final AccountHolder accountHolder = getAccountHolder(account);
        final boolean correctPassword = password.equals(accountHolder.getPassword());
        return runTask(mExecutor,
                new AccountManagerTask<Bundle>(handler, callback, new Callable<Bundle>() {
            @Override
            public Bundle call() throws Exception {
                Bundle result = new Bundle();
                result.putString(AccountManager.KEY_ACCOUNT_NAME, accountHolder.getAccount().name);
                result.putString(
                        AccountManager.KEY_ACCOUNT_TYPE, AccountManagerHelper.GOOGLE_ACCOUNT_TYPE);
                result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, correctPassword);
                return result;
            }
        }));
    }

    @Override
    public String blockingGetAuthToken(Account account, String authTokenType,
            boolean notifyAuthFailure)
            throws OperationCanceledException, IOException, AuthenticatorException {
        AccountHolder accountHolder = getAccountHolder(account);
        if (accountHolder.hasBeenAccepted(authTokenType)) {
            // If account has already been accepted we can just return the auth token.
            return internalGenerateAndStoreAuthToken(accountHolder, authTokenType);
        }
        AccountAuthTokenPreparation prepared = getPreparedPermission(account, authTokenType);
        Intent intent = newGrantCredentialsPermissionIntent(false, account, authTokenType);
        waitForActivity(mContext, intent);
        applyPreparedPermission(prepared);
        return internalGenerateAndStoreAuthToken(accountHolder, authTokenType);
    }

    @Override
    public AccountManagerFuture<Bundle> getAuthToken(Account account, String authTokenType,
            Bundle options, Activity activity, AccountManagerCallback<Bundle> callback,
            Handler handler) {
        return getAuthTokenFuture(account, authTokenType, activity, callback, handler);
    }

    @Override
    public AccountManagerFuture<Bundle> getAuthToken(Account account, String authTokenType,
            boolean notifyAuthFailure, AccountManagerCallback<Bundle> callback, Handler handler) {
        return getAuthTokenFuture(account, authTokenType, null, callback, handler);
    }

    private AccountManagerFuture<Bundle> getAuthTokenFuture(Account account, String authTokenType,
            Activity activity, AccountManagerCallback<Bundle> callback, Handler handler) {
        final AccountHolder ah = getAccountHolder(account);
        if (ah.hasBeenAccepted(authTokenType)) {
            final String authToken = internalGenerateAndStoreAuthToken(ah, authTokenType);
            return runTask(mExecutor,
                    new AccountManagerAuthTokenTask(activity, handler, callback,
                            account, authTokenType,
                            new Callable<Bundle>() {
                        @Override
                        public Bundle call() throws Exception {
                            return getAuthTokenBundle(ah.getAccount(), authToken);
                        }
                    }));
        } else {
            Log.d(TAG, "getAuthTokenFuture: Account " + ah.getAccount() +
                    " is asking for permission for " + authTokenType);
            final Intent intent = newGrantCredentialsPermissionIntent(true, account, authTokenType);
            return runTask(mExecutor,
                    new AccountManagerAuthTokenTask(activity, handler, callback,
                            account, authTokenType,
                            new Callable<Bundle>() {
                        @Override
                        public Bundle call() throws Exception {
                            Bundle result = new Bundle();
                            result.putParcelable(AccountManager.KEY_INTENT, intent);
                            return result;
                        }
                    }));
        }
    }

    private static Bundle getAuthTokenBundle(Account account, String authToken) {
        Bundle result = new Bundle();
        result.putString(AccountManager.KEY_AUTHTOKEN, authToken);
        result.putString(AccountManager.KEY_ACCOUNT_NAME, account.name);
        result.putString(AccountManager.KEY_ACCOUNT_TYPE, account.type);
        return result;
    }

    private String internalGenerateAndStoreAuthToken(AccountHolder ah, String authTokenType) {
        synchronized (mAccounts) {
            if (ah.getAuthToken(authTokenType) == null) {
                // No authtoken registered. Need to create one.
                String authToken = UUID.randomUUID().toString();
                Log.d(TAG, "Created new auth token for " + ah.getAccount() +
                        ": autTokenType = " + authTokenType + ", authToken = " + authToken);
                ah = ah.withAuthToken(authTokenType, authToken);
                mAccounts.add(ah);
            }
        }
        return ah.getAuthToken(authTokenType);
    }

    @Override
    public String peekAuthToken(Account account, String authTokenType) {
        return getAccountHolder(account).getAuthToken(authTokenType);
    }

    @Override
    public void invalidateAuthToken(String accountType, String authToken) {
        if(!AccountManagerHelper.GOOGLE_ACCOUNT_TYPE.equals(accountType)) {
            throw new IllegalArgumentException("Invalid account type: " + accountType);
        }
        if (authToken == null) {
            throw new IllegalArgumentException("AuthToken can not be null");
        }
        for (AccountHolder ah : mAccounts) {
            if (ah.removeAuthToken(authToken)) {
                break;
            }
        }
    }

    public void prepareAllowAppPermission(Account account, String authTokenType) {
        addPreparedAppPermission(new AccountAuthTokenPreparation(account, authTokenType, true));
    }

    public void prepareDenyAppPermission(Account account, String authTokenType) {
        addPreparedAppPermission(new AccountAuthTokenPreparation(account, authTokenType, false));
    }

    private void addPreparedAppPermission(AccountAuthTokenPreparation accountAuthTokenPreparation) {
        Log.d(TAG, "Adding " + accountAuthTokenPreparation);
        mAccountPermissionPreparations.add(accountAuthTokenPreparation);
    }

    private AccountAuthTokenPreparation getPreparedPermission(Account account,
            String authTokenType) {
        for (AccountAuthTokenPreparation accountPrep : mAccountPermissionPreparations) {
            if (accountPrep.getAccount().equals(account) &&
                    accountPrep.getAuthTokenType().equals(authTokenType)) {
                return accountPrep;
            }
        }
        return null;
    }

    private void applyPreparedPermission(AccountAuthTokenPreparation prep) {
        if (prep != null) {
            Log.d(TAG, "Applying " + prep);
            mAccountPermissionPreparations.remove(prep);
            mAccounts.add(getAccountHolder(prep.getAccount()).withHasBeenAccepted(
                    prep.getAuthTokenType(), prep.isAllowed()));
        }
    }

    private Intent newGrantCredentialsPermissionIntent(boolean hasActivity, Account account,
            String authTokenType) {
        Intent intent = new Intent();
        intent.setComponent(new ComponentName(mTestContext,
                MockGrantCredentialsPermissionActivity.class.getCanonicalName()));
        intent.putExtra(MockGrantCredentialsPermissionActivity.ACCOUNT, account);
        intent.putExtra(MockGrantCredentialsPermissionActivity.AUTH_TOKEN_TYPE, authTokenType);
        if (!hasActivity) {
            // No activity provided, so we help the caller by adding the new task flag
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        }
        return intent;
    }

    private AccountHolder getAccountHolder(Account account) {
        if (account == null) {
            throw new IllegalArgumentException("Account can not be null");
        }
        for (AccountHolder accountHolder : mAccounts) {
            if (account.equals(accountHolder.getAccount())) {
                return accountHolder;
            }
        }
        throw new IllegalArgumentException("Can not find AccountHolder for account " + account);
    }

    private static <T> AccountManagerFuture<T> runTask(Executor executorService,
            AccountManagerTask<T> accountManagerBundleTask) {
        executorService.execute(accountManagerBundleTask);
        return accountManagerBundleTask;
    }

    private class AccountManagerTask<T> extends FutureTask<T> implements AccountManagerFuture<T> {

        protected final Handler mHandler;

        protected final AccountManagerCallback<T> mCallback;

        protected final Callable<T> mCallable;

        public AccountManagerTask(Handler handler,
                AccountManagerCallback<T> callback, Callable<T> callable) {
            super(new Callable<T>() {
                @Override
                public T call() throws Exception {
                    throw new IllegalStateException("this should never be called, "
                            + "but call must be overridden.");
                }
            });
            mHandler = handler;
            mCallback = callback;
            mCallable = callable;
        }

        private T internalGetResult(long timeout, TimeUnit unit)
                throws OperationCanceledException, IOException, AuthenticatorException {
            try {
                if (timeout == -1) {
                    return get();
                } else {
                    return get(timeout, unit);
                }
            } catch (CancellationException e) {
                throw new OperationCanceledException();
            } catch (TimeoutException e) {
                // Fall through and cancel.
            } catch (InterruptedException e) {
                // Fall through and cancel.
            } catch (ExecutionException e) {
                final Throwable cause = e.getCause();
                if (cause instanceof IOException) {
                    throw (IOException) cause;
                } else if (cause instanceof UnsupportedOperationException) {
                    throw new AuthenticatorException(cause);
                } else if (cause instanceof AuthenticatorException) {
                    throw (AuthenticatorException) cause;
                } else if (cause instanceof RuntimeException) {
                    throw (RuntimeException) cause;
                } else if (cause instanceof Error) {
                    throw (Error) cause;
                } else {
                    throw new IllegalStateException(cause);
                }
            } finally {
                cancel(true /* Interrupt if running. */);
            }
            throw new OperationCanceledException();
        }

        @Override
        public T getResult()
                throws OperationCanceledException, IOException, AuthenticatorException {
            return internalGetResult(-1, null);
        }

        @Override
        public T getResult(long timeout, TimeUnit unit)
                throws OperationCanceledException, IOException, AuthenticatorException {
            return internalGetResult(timeout, unit);
        }

        @Override
        public void run() {
            try {
                set(mCallable.call());
            } catch (Exception e) {
                setException(e);
            }
        }

        @Override
        protected void done() {
            if (mCallback != null) {
                postToHandler(getHandler(), mCallback, this);
            }
        }

        protected Handler getHandler() {
            return mHandler == null ? mMainHandler : mHandler;
        }

    }

    private static <T> void postToHandler(Handler handler, final AccountManagerCallback<T> callback,
            final AccountManagerFuture<T> future) {
        handler.post(new Runnable() {
            @Override
            public void run() {
                callback.run(future);
            }
        });
    }

    private class AccountManagerAuthTokenTask extends AccountManagerTask<Bundle> {

        private final Activity mActivity;

        private final AccountAuthTokenPreparation mAccountAuthTokenPreparation;

        private final Account mAccount;

        private final String mAuthTokenType;

        public AccountManagerAuthTokenTask(Activity activity, Handler handler,
                AccountManagerCallback<Bundle> callback,
                Account account, String authTokenType,
                Callable<Bundle> callable) {
            super(handler, callback, callable);
            mActivity = activity;
            mAccountAuthTokenPreparation = getPreparedPermission(account, authTokenType);
            mAccount = account;
            mAuthTokenType = authTokenType;
        }

        @Override
        public void run() {
            try {
                Bundle bundle = mCallable.call();
                Intent intent = bundle.getParcelable(AccountManager.KEY_INTENT);
                if (intent != null && mActivity != null) {
                    // Since the user provided an Activity we will silently start intents we see.
                    // Starting activity and waiting for it to finish.
                    waitForActivity(mActivity, intent);
                    if (mAccountAuthTokenPreparation == null) {
                        throw new IllegalStateException("No account preparation ready for " +
                                mAccount + ", authTokenType = " + mAuthTokenType +
                                ". Add a call to either prepareGrantAppPermission(...) or " +
                                "prepareRevokeAppPermission(...) in your test before asking for " +
                                "an auth token");
                    } else {
                        // We have shown the Allow/Deny activity, and it has gone away. We can now
                        // apply the pre-stored permission.
                        applyPreparedPermission(mAccountAuthTokenPreparation);
                        generateResult(getAccountHolder(mAccount), mAuthTokenType);
                    }
                } else {
                    set(bundle);
                }
            } catch (Exception e) {
                setException(e);
            }
        }

        private void generateResult(AccountHolder accountHolder, String authTokenType)
                throws OperationCanceledException {
            if (accountHolder.hasBeenAccepted(authTokenType)) {
                String authToken = internalGenerateAndStoreAuthToken(accountHolder, authTokenType);
                // Return a valid auth token.
                set(getAuthTokenBundle(accountHolder.getAccount(), authToken));
            } else {
                // Throw same exception as when user clicks "Deny".
                throw new OperationCanceledException("User denied request");
            }
        }
    }

    /**
     * This method starts {@link MockGrantCredentialsPermissionActivity} and waits for it
     * to be started before it returns.
     *
     * @param context the context to start the intent in
     * @param intent the intent to use to start MockGrantCredentialsPermissionActivity
     */
    private void waitForActivity(Context context, Intent intent) {
        final Object mutex = new Object();
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                synchronized (mutex) {
                    mutex.notifyAll();
                }
            }
        };
        if (!MockGrantCredentialsPermissionActivity.class.getCanonicalName().
                equals(intent.getComponent().getClassName())) {
            throw new IllegalArgumentException("Can only wait for "
                    + "MockGrantCredentialsPermissionActivity");
        }
        mContext.registerReceiver(receiver, new IntentFilter(MUTEX_WAIT_ACTION));
        context.startActivity(intent);
        try {
            Log.d(TAG, "Waiting for broadcast of " + MUTEX_WAIT_ACTION);
            synchronized (mutex) {
                mutex.wait(WAIT_TIME_FOR_GRANT_BROADCAST_MS);
            }
        } catch (InterruptedException e) {
            throw new IllegalStateException("Got unexpected InterruptedException");
        }
        Log.d(TAG, "Got broadcast of " + MUTEX_WAIT_ACTION);
        mContext.unregisterReceiver(receiver);
    }

    private void postAsyncAccountChangedEvent() {
        // Mimic that this does not happen on the main thread.
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                mContext.sendBroadcast(new Intent(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION));
                return null;
            }
        }.execute();
    }

    /**
     * Internal class for storage of prepared account auth token permissions.
     *
     * This is used internally by {@link MockAccountManager} to mock the same behavior as clicking
     * Allow/Deny in the Android {@link GrantCredentialsPermissionActivity}.
     */
    private static class AccountAuthTokenPreparation {

        private final Account mAccount;

        private final String mAuthTokenType;

        private final boolean mAllowed;

        private AccountAuthTokenPreparation(Account account, String authTokenType,
                boolean allowed) {
            mAccount = account;
            mAuthTokenType = authTokenType;
            mAllowed = allowed;
        }

        public Account getAccount() {
            return mAccount;
        }

        public String getAuthTokenType() {
            return mAuthTokenType;
        }

        public boolean isAllowed() {
            return mAllowed;
        }

        @Override
        public String toString() {
            return "AccountAuthTokenPreparation{" +
                    "mAccount=" + mAccount +
                    ", mAuthTokenType='" + mAuthTokenType + '\'' +
                    ", mAllowed=" + mAllowed +
                    '}';
        }
    }
}
