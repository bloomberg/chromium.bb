// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AuthenticatorDescription;
import android.app.Activity;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.common.base.Optional;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.signin.AccountManagerDelegate.CapabilityResponse;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

/**
 * AccountManagerFacade wraps our access of AccountManager in Android.
 */
public class AccountManagerFacadeImpl implements AccountManagerFacade {
    /**
     * An account feature (corresponding to a Gaia service flag) that specifies whether the account
     * is a child account.
     */
    @VisibleForTesting
    public static final String FEATURE_IS_CHILD_ACCOUNT_KEY = "service_uca";

    /**
     * An account feature (corresponding to a Gaia service flag) that specifies whether the account
     * is a USM account.
     */
    @VisibleForTesting
    public static final String FEATURE_IS_USM_ACCOUNT_KEY = "service_usm";

    @VisibleForTesting
    static final String CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS = "gi2tklldmfya";

    private final AccountManagerDelegate mDelegate;
    private final AccountRestrictionPatternReceiver mAccountRestrictionPatternReceiver;

    private final ObserverList<AccountsChangeObserver> mObservers = new ObserverList<>();

    private final AtomicReference<List<Account>> mAllAccounts = new AtomicReference<>();
    private final AtomicReference<List<PatternMatcher>> mAccountRestrictionPatterns =
            new AtomicReference<>();

    // The map stores the boolean for whether an account can offer extended chrome sync promos
    private final AtomicReference<Map<String, Boolean>> mCanOfferExtendedSyncPromos =
            new AtomicReference<>(new HashMap<>());

    private @NonNull Promise<List<Account>> mAccountsPromise = new Promise<>();

    /**
     * @param delegate the AccountManagerDelegate to use as a backend
     */
    public AccountManagerFacadeImpl(AccountManagerDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        mDelegate = delegate;
        mDelegate.attachAccountsChangeObserver(this::onAccountsUpdated);
        mAccountRestrictionPatternReceiver =
                new AccountRestrictionPatternReceiver(this::onAccountRestrictionPatternsUpdated);

        getAccounts().then(accounts -> {
            RecordHistogram.recordExactLinearHistogram(
                    "Signin.AndroidNumberOfDeviceAccounts", accounts.size(), 50);
        });
        new InitializeTask().executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    /**
     * Adds an observer to receive accounts change notifications.
     * @param observer the observer to add.
     */
    @Override
    public void addObserver(AccountsChangeObserver observer) {
        ThreadUtils.assertOnUiThread();
        boolean success = mObservers.addObserver(observer);
        assert success : "Observer already added!";
    }

    /**
     * Removes an observer that was previously added using {@link #addObserver}.
     * @param observer the observer to remove.
     */
    @Override
    public void removeObserver(AccountsChangeObserver observer) {
        ThreadUtils.assertOnUiThread();
        boolean success = mObservers.removeObserver(observer);
        assert success : "Can't find observer";
    }

    @Override
    public Promise<List<Account>> getAccounts() {
        ThreadUtils.assertOnUiThread();
        return mAccountsPromise;
    }

    /**
     * @return Whether or not there is an account authenticator for Google accounts.
     */
    @Override
    public boolean hasGoogleAccountAuthenticator() {
        AuthenticatorDescription[] descs = mDelegate.getAuthenticatorTypes();
        for (AuthenticatorDescription desc : descs) {
            if (AccountUtils.GOOGLE_ACCOUNT_TYPE.equals(desc.type)) return true;
        }
        return false;
    }

    /**
     * Synchronously gets an OAuth2 access token. May return a cached version, use
     * {@link #invalidateAccessToken} to invalidate a token in the cache.
     * @param account The {@link Account} for which the token is requested.
     * @param scope OAuth2 scope for which the requested token should be valid.
     * @return The OAuth2 access token as an AccessTokenData with a string and an expiration time..
     */
    @Override
    public AccessTokenData getAccessToken(Account account, String scope) throws AuthException {
        assert account != null;
        assert scope != null;
        return mDelegate.getAuthToken(account, scope);
    }

    /**
     * Removes an OAuth2 access token from the cache with retries asynchronously.
     * Uses {@link #getAccessToken} to issue a new token after invalidating the old one.
     * @param accessToken The access token to invalidate.
     */
    @Override
    public void invalidateAccessToken(String accessToken) {
        if (!TextUtils.isEmpty(accessToken)) {
            ConnectionRetry.runAuthTask(() -> {
                mDelegate.invalidateAuthToken(accessToken);
                return true;
            });
        }
    }

    @Override
    public void checkChildAccountStatus(Account account, ChildAccountStatusListener listener) {
        ThreadUtils.assertOnUiThread();
        new AsyncTask<Integer>() {
            @Override
            public @ChildAccountStatus.Status Integer doInBackground() {
                if (mDelegate.hasFeature(account, FEATURE_IS_CHILD_ACCOUNT_KEY)) {
                    return ChildAccountStatus.REGULAR_CHILD;
                } else if (mDelegate.hasFeature(account, FEATURE_IS_USM_ACCOUNT_KEY)) {
                    return ChildAccountStatus.USM_CHILD;
                } else {
                    return ChildAccountStatus.NOT_CHILD;
                }
            }

            @Override
            public void onPostExecute(@ChildAccountStatus.Status Integer status) {
                listener.onStatusReady(status);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Override
    public Optional<Boolean> canOfferExtendedSyncPromos(Account account) {
        return Optional.fromNullable(
                mCanOfferExtendedSyncPromos.get().get(AccountUtils.canonicalizeName(account.name)));
    }

    /**
     * Creates an intent that will ask the user to add a new account to the device. See
     * {@link AccountManager#addAccount} for details.
     * @param callback The callback to get the created intent. Will be invoked on the main thread.
     *         If there is an issue while creating the intent, callback will receive null.
     */
    @Override
    public void createAddAccountIntent(Callback<Intent> callback) {
        mDelegate.createAddAccountIntent(callback);
    }

    /**
     * Asks the user to enter a new password for an account, updating the saved credentials for the
     * account.
     */
    @Override
    public void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback) {
        mDelegate.updateCredentials(account, activity, callback);
    }

    /**
     * Gets profile data source.
     * @return {@link ProfileDataSource} if it is supported by implementation, null otherwise.
     */
    @Override
    public ProfileDataSource getProfileDataSource() {
        return mDelegate.getProfileDataSource();
    }

    /**
     * Returns the Gaia id for the account associated with the given email address.
     * If an account with the given email address is not installed on the device
     * then null is returned.
     *
     * This method will throw IllegalStateException if called on the main thread.
     *
     * @param accountEmail The email address of a Google account.
     */
    @Override
    public String getAccountGaiaId(String accountEmail) {
        return mDelegate.getAccountGaiaId(accountEmail);
    }

    private void updateCanOfferExtendedSyncPromos(List<Account> accounts) {
        PostTask.postTask(TaskTraits.USER_VISIBLE, () -> {
            final Map<String, Boolean> canOfferExtendedSyncPromos = new HashMap<>();
            for (Account account : accounts) {
                canOfferExtendedSyncPromos.put(AccountUtils.canonicalizeName(account.name),
                        mDelegate.hasCapability(account, CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS)
                                == CapabilityResponse.YES);
            }
            mCanOfferExtendedSyncPromos.set(canOfferExtendedSyncPromos);
        });
    }

    private void onAccountsUpdated() {
        ThreadUtils.assertOnUiThread();
        new UpdateAccountsTask().executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private void onAccountRestrictionPatternsUpdated(List<PatternMatcher> patternMatchers) {
        mAccountRestrictionPatterns.set(patternMatchers);
        updateAccounts();
    }

    @MainThread
    private void updateAccounts() {
        final List<Account> newAccounts = getFilteredAccounts();
        updateCanOfferExtendedSyncPromos(newAccounts);
        if (mAccountsPromise.isFulfilled()) {
            mAccountsPromise = Promise.fulfilled(newAccounts);
        } else {
            mAccountsPromise.fulfill(newAccounts);
        }
        for (AccountsChangeObserver observer : mObservers) {
            observer.onAccountsChanged();
        }
    }

    private List<Account> getFilteredAccounts() {
        if (mAccountRestrictionPatterns.get().isEmpty()) {
            return mAllAccounts.get();
        }
        final List<Account> filteredAccounts = new ArrayList<>();
        for (Account account : mAllAccounts.get()) {
            for (PatternMatcher pattern : mAccountRestrictionPatterns.get()) {
                if (pattern.matches(account.name)) {
                    filteredAccounts.add(account);
                    break; // Don't check other patterns
                }
            }
        }
        return Collections.unmodifiableList(filteredAccounts);
    }

    private List<Account> getAllAccounts() {
        return Collections.unmodifiableList(Arrays.asList(mDelegate.getAccounts()));
    }

    private class InitializeTask extends AsyncTask<Void> {
        @Override
        protected Void doInBackground() {
            mAccountRestrictionPatterns.set(
                    mAccountRestrictionPatternReceiver.getRestrictionPatterns());
            mAllAccounts.set(getAllAccounts());
            return null;
        }

        @Override
        protected void onPostExecute(Void v) {
            updateAccounts();
        }
    }

    private class UpdateAccountsTask extends AsyncTask<List<Account>> {
        @Override
        protected List<Account> doInBackground() {
            return getAllAccounts();
        }

        @Override
        protected void onPostExecute(List<Account> allAccounts) {
            mAllAccounts.set(allAccounts);
            updateAccounts();
        }
    }
}
