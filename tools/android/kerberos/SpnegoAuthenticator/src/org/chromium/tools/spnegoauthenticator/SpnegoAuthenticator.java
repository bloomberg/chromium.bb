// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.spnegoauthenticator;

import android.accounts.AbstractAccountAuthenticator;
import android.accounts.Account;
import android.accounts.AccountAuthenticatorResponse;
import android.accounts.AccountManager;
import android.accounts.NetworkErrorException;
import android.content.Context;
import android.os.Bundle;

import org.chromium.base.Log;
import org.chromium.net.HttpNegotiateConstants;

import java.util.Arrays;

/**
 * AccountAuthenticator implementation that automatically creates a dummy account and returns a
 * dummy token when asked.
 */
public class SpnegoAuthenticator extends AbstractAccountAuthenticator {
    private static final String TAG = "tools_SpnegoAuth";
    private static final String ACCOUNT_NAME = "DummySpnegoAccount";

    /**
     * @param context
     */
    public SpnegoAuthenticator(Context context) {
        super(context);
        ensureTestAccountExists(context);
    }

    @Override
    public Bundle addAccount(AccountAuthenticatorResponse arg0, String accountType, String arg2,
            String[] arg3, Bundle arg4) throws NetworkErrorException {
        Log.w(TAG, "addAccount():  Not supported.");
        Bundle result = new Bundle();
        result.putInt(AccountManager.KEY_ERROR_CODE, AccountManager.ERROR_CODE_BAD_REQUEST);
        result.putString(AccountManager.KEY_ERROR_MESSAGE, "Can't add new SPNEGO accounts");
        return result;
    }

    @Override
    public Bundle confirmCredentials(AccountAuthenticatorResponse arg0, Account arg1, Bundle arg2)
            throws NetworkErrorException {
        Bundle result = new Bundle();
        result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, true);
        return result;
    }

    @Override
    public Bundle editProperties(AccountAuthenticatorResponse arg0, String arg1) {
        return new Bundle();
    }

    @Override
    public Bundle getAuthToken(AccountAuthenticatorResponse response, Account account,
            String authTokenType, Bundle options) throws NetworkErrorException {
        Bundle result = new Bundle();
        result.putString(AccountManager.KEY_ACCOUNT_NAME, account.name);
        result.putString(AccountManager.KEY_ACCOUNT_TYPE, account.type);
        result.putString(AccountManager.KEY_AUTHTOKEN, "DummyAuthToken");
        result.putInt(HttpNegotiateConstants.KEY_SPNEGO_RESULT, 0);
        Log.d(TAG, "getAuthToken(): Returning dummy SPNEGO auth token");
        return result;
    }

    @Override
    public String getAuthTokenLabel(String authTokenType) {
        return "Spnego " + authTokenType;
    }

    @Override
    public Bundle hasFeatures(AccountAuthenticatorResponse arg0, Account arg1, String[] features)
            throws NetworkErrorException {
        Log.d(TAG, "hasFeatures(%s)", Arrays.asList(features));
        Bundle result = new Bundle();
        for (String feature : features) {
            if (!feature.equals(HttpNegotiateConstants.SPNEGO_FEATURE)) {
                result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, false);
                return result;
            }
        }
        result.putBoolean(AccountManager.KEY_BOOLEAN_RESULT, true);
        return result;
    }

    @Override
    public Bundle updateCredentials(AccountAuthenticatorResponse arg0, Account arg1, String arg2,
            Bundle arg3) throws NetworkErrorException {
        Log.w(TAG, "updateCredentials(): Not supported.");
        Bundle result = new Bundle();
        result.putInt(AccountManager.KEY_ERROR_CODE, AccountManager.ERROR_CODE_BAD_REQUEST);
        result.putString(AccountManager.KEY_ERROR_MESSAGE, "Can't update credentials.");
        return result;
    }

    private void ensureTestAccountExists(Context context) {
        AccountManager am = AccountManager.get(context);
        Account account = new Account(ACCOUNT_NAME, context.getString(R.string.account_type));
        am.addAccountExplicitly(account, null, null);
    }
}
