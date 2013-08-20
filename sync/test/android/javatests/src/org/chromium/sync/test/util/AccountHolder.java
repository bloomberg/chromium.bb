// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.test.util;

import android.accounts.Account;

import java.util.HashMap;
import java.util.Map;

/**
 * This class is used by the {@link MockAccountManager} to hold information about a given
 * account, such as its password and set of granted auth tokens.
 */
public class AccountHolder {

    private final Account mAccount;

    private final String mPassword;

    private final Map<String, String> mAuthTokens;

    private final Map<String, Boolean> mHasBeenAccepted;

    private final boolean mAlwaysAccept;

    private AccountHolder(Account account, String password, Map<String, String> authTokens,
            Map<String, Boolean> hasBeenAccepted, boolean alwaysAccept) {
        mAlwaysAccept = alwaysAccept;
        if (account == null) {
            throw new IllegalArgumentException("Account can not be null");
        }
        mAccount = account;
        mPassword = password;
        mAuthTokens = authTokens == null ? new HashMap<String, String>() : authTokens;
        mHasBeenAccepted = hasBeenAccepted == null ?
                new HashMap<String, Boolean>() : hasBeenAccepted;
    }

    public Account getAccount() {
        return mAccount;
    }

    public String getPassword() {
        return mPassword;
    }

    public boolean hasAuthTokenRegistered(String authTokenType) {
        return mAuthTokens.containsKey(authTokenType);
    }

    public String getAuthToken(String authTokenType) {
        return mAuthTokens.get(authTokenType);
    }

    public boolean hasBeenAccepted(String authTokenType) {
        return mAlwaysAccept ||
                mHasBeenAccepted.containsKey(authTokenType) && mHasBeenAccepted.get(authTokenType);
    }

    /**
     * Removes an auth token from the auth token map.
     *
     * @param authToken the auth token to remove
     * @return true if the auth token was found
     */
    public boolean removeAuthToken(String authToken) {
        String foundKey = null;
        for (Map.Entry<String, String> tokenEntry : mAuthTokens.entrySet()) {
            if (authToken.equals(tokenEntry.getValue())) {
                foundKey = tokenEntry.getKey();
                break;
            }
        }
        if (foundKey == null) {
            return false;
        } else {
            mAuthTokens.remove(foundKey);
            return true;
        }
    }

    @Override
    public int hashCode() {
        return mAccount.hashCode();
    }

    @Override
    public boolean equals(Object that) {
        return that != null && that instanceof AccountHolder && mAccount
                .equals(((AccountHolder) that).getAccount());
    }

    public static Builder create() {
        return new Builder();
    }

    public AccountHolder withPassword(String password) {
        return copy().password(password).build();
    }

    public AccountHolder withAuthTokens(Map<String, String> authTokens) {
        return copy().authTokens(authTokens).build();
    }

    public AccountHolder withAuthToken(String authTokenType, String authToken) {
        return copy().authToken(authTokenType, authToken).build();
    }

    public AccountHolder withHasBeenAccepted(String authTokenType, boolean hasBeenAccepted) {
        return copy().hasBeenAccepted(authTokenType, hasBeenAccepted).build();
    }

    public AccountHolder withAlwaysAccept(boolean alwaysAccept) {
        return copy().alwaysAccept(alwaysAccept).build();
    }

    private Builder copy() {
        return create().account(mAccount).password(mPassword).authTokens(mAuthTokens).
                hasBeenAcceptedMap(mHasBeenAccepted).alwaysAccept(mAlwaysAccept);
    }

    public static class Builder {

        private Account mTempAccount;

        private String mTempPassword;

        private Map<String, String> mTempAuthTokens;

        private Map<String, Boolean> mTempHasBeenAccepted;

        private boolean mTempAlwaysAccept;

        public Builder account(Account account) {
            mTempAccount = account;
            return this;
        }

        public Builder password(String password) {
            mTempPassword = password;
            return this;
        }

        public Builder authToken(String authTokenType, String authToken) {
            if (mTempAuthTokens == null) {
                mTempAuthTokens = new HashMap<String, String>();
            }
            mTempAuthTokens.put(authTokenType, authToken);
            return this;
        }

        public Builder authTokens(Map<String, String> authTokens) {
            mTempAuthTokens = authTokens;
            return this;
        }

        public Builder hasBeenAccepted(String authTokenType, boolean hasBeenAccepted) {
            if (mTempHasBeenAccepted == null) {
                mTempHasBeenAccepted = new HashMap<String, Boolean>();
            }
            mTempHasBeenAccepted.put(authTokenType, hasBeenAccepted);
            return this;
        }

        public Builder hasBeenAcceptedMap(Map<String, Boolean> hasBeenAcceptedMap) {
            mTempHasBeenAccepted = hasBeenAcceptedMap;
            return this;
        }

        public Builder alwaysAccept(boolean alwaysAccept) {
            mTempAlwaysAccept = alwaysAccept;
            return this;
        }

        public AccountHolder build() {
            return new AccountHolder(mTempAccount, mTempPassword, mTempAuthTokens,
                    mTempHasBeenAccepted, mTempAlwaysAccept);
        }
    }

}
