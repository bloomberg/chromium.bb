// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test.util;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;

import androidx.annotation.GuardedBy;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import com.google.common.base.Optional;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.ProfileDataSource;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;

/**
 * FakeAccountManagerFacade is an {@link AccountManagerFacade} stub intended
 * for testing.
 */
public class FakeAccountManagerFacade implements AccountManagerFacade {
    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private final Set<AccountHolder> mAccountHolders = new LinkedHashSet<>();

    private final List<AccountsChangeObserver> mObservers = new ArrayList<>();

    private final @Nullable FakeProfileDataSource mFakeProfileDataSource;

    /**
     * Creates an object of FakeAccountManagerFacade.
     * @param fakeProfileDataSource A FakeProfileDataSource instance if needed.
     */
    public FakeAccountManagerFacade(@Nullable FakeProfileDataSource fakeProfileDataSource) {
        mFakeProfileDataSource = fakeProfileDataSource;
    }

    @Override
    @Nullable
    public ProfileDataSource getProfileDataSource() {
        return mFakeProfileDataSource;
    }

    @MainThread
    @Override
    public void addObserver(AccountsChangeObserver observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.add(observer);
    }

    @MainThread
    @Override
    public void removeObserver(AccountsChangeObserver observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.remove(observer);
    }

    @Override
    public Promise<List<Account>> getAccounts() {
        List<Account> accounts = new ArrayList<>();
        synchronized (mLock) {
            for (AccountHolder accountHolder : mAccountHolders) {
                accounts.add(accountHolder.getAccount());
            }
        }
        return Promise.fulfilled(accounts);
    }

    @Override
    public boolean hasGoogleAccountAuthenticator() {
        return true;
    }

    @Override
    public AccessTokenData getAccessToken(Account account, String scope) {
        synchronized (mLock) {
            AccountHolder accountHolder = getAccountHolder(account);
            if (accountHolder.getAuthToken(scope) == null) {
                accountHolder.updateAuthToken(scope, UUID.randomUUID().toString());
            }
            return accountHolder.getAuthToken(scope);
        }
    }

    @Override
    public void invalidateAccessToken(String accessToken) {
        synchronized (mLock) {
            for (AccountHolder accountHolder : mAccountHolders) {
                if (accountHolder.removeAuthToken(accessToken)) {
                    break;
                }
            }
        }
    }

    @Override
    public void checkChildAccountStatus(Account account, ChildAccountStatusListener listener) {}

    @Override
    public Optional<Boolean> canOfferExtendedSyncPromos(Account account) {
        assert account != null;
        return Optional.absent();
    }

    @Override
    public void createAddAccountIntent(Callback<Intent> callback) {}

    @Override
    public void updateCredentials(
            Account account, Activity activity, @Nullable Callback<Boolean> callback) {}

    @Override
    public String getAccountGaiaId(String accountEmail) {
        return toGaiaId(accountEmail);
    }

    /**
     * Adds an account to the fake AccountManagerFacade.
     */
    public void addAccount(Account account) {
        AccountHolder accountHolder = AccountHolder.createFromAccount(account);
        // As this class is accessed both from UI thread and worker threads, we lock the access
        // to account holders to avoid potential race condition.
        synchronized (mLock) {
            mAccountHolders.add(accountHolder);
        }
        ThreadUtils.runOnUiThreadBlocking(this::fireOnAccountsChangedNotification);
    }

    /**
     * Removes an account from the fake AccountManagerFacade.
     */
    public void removeAccount(Account account) {
        AccountHolder accountHolder = AccountHolder.createFromAccount(account);
        synchronized (mLock) {
            if (!mAccountHolders.remove(accountHolder)) {
                throw new IllegalArgumentException("Cannot find account:" + accountHolder);
            }
        }
        ThreadUtils.runOnUiThreadBlocking(this::fireOnAccountsChangedNotification);
    }

    /**
     * Adds a {@link ProfileDataSource.ProfileData} to the FakeProfileDataSource.
     */
    public void addProfileData(ProfileDataSource.ProfileData profileData) {
        assert mFakeProfileDataSource != null : "ProfileDataSource was disabled!";
        mFakeProfileDataSource.addProfileData(profileData);
    }

    /**
     * Converts an email to a fake gaia Id.
     */
    public static String toGaiaId(String email) {
        return "gaia-id-" + email.replace("@", "_at_");
    }

    @GuardedBy("mLock")
    private AccountHolder getAccountHolder(Account account) {
        for (AccountHolder accountHolder : mAccountHolders) {
            if (accountHolder.getAccount().equals(account)) {
                return accountHolder;
            }
        }
        throw new IllegalArgumentException("Cannot find account:" + account);
    }

    @MainThread
    private void fireOnAccountsChangedNotification() {
        for (AccountsChangeObserver observer : mObservers) {
            observer.onAccountsChanged();
        }
    }
}
