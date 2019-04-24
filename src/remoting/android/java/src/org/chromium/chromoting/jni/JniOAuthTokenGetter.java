// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chromoting.Preconditions;
import org.chromium.chromoting.base.OAuthTokenFetcher;
import org.chromium.chromoting.base.OAuthTokenFetcher.Callback;
import org.chromium.chromoting.base.OAuthTokenFetcher.Error;

/**
 * The Java implementation of the JniOAuthTokenGetter class. Used by C++ code to request OAuth
 * token.
 * Note that both context and account must be set before the token getter is being used.
 */
@JNINamespace("remoting")
public class JniOAuthTokenGetter {
    private static final String TAG = "Chromoting";
    private static final String TOKEN_SCOPE = "oauth2:https://www.googleapis.com/auth/chromoting";

    private static String sAccount;
    private static String sLatestToken;

    public static void setAccount(String account) {
        Preconditions.notNull(account);
        sAccount = account;
    }

    @CalledByNative
    private static void fetchAuthToken(long callbackPtr) {
        Preconditions.notNull(ContextUtils.getApplicationContext());
        Preconditions.notNull(sAccount);
        new OAuthTokenFetcher(ContextUtils.getApplicationContext(), sAccount, TOKEN_SCOPE,
                new Callback() {
                    @Override
                    public void onTokenFetched(String token) {
                        sLatestToken = token;
                        nativeResolveOAuthTokenCallback(
                                callbackPtr, OAuthTokenStatus.SUCCESS, sAccount, token);
                    }

                    @Override
                    public void onError(@Error int error) {
                        Log.e(TAG, "Failed to fetch token. Error: ", error);
                        int status;
                        switch (error) {
                            case Error.NETWORK:
                                status = OAuthTokenStatus.NETWORK_ERROR;
                                break;
                            case Error.UI:
                            case Error.UNEXPECTED:
                            case Error.INTERRUPTED:
                                status = OAuthTokenStatus.AUTH_ERROR;
                                break;
                            default:
                                assert false : "Unreached";
                                status = -1;
                        }
                        nativeResolveOAuthTokenCallback(callbackPtr, status, null, null);
                    }
                })
                .fetch();
    }

    @CalledByNative
    private static void invalidateCache() {
        if (sLatestToken == null || sLatestToken.isEmpty()) {
            return;
        }
        Preconditions.notNull(ContextUtils.getApplicationContext());
        Preconditions.notNull(sAccount);
        new OAuthTokenFetcher(ContextUtils.getApplicationContext(), sAccount, TOKEN_SCOPE,
                new Callback() {
                    @Override
                    public void onTokenFetched(String token) {
                        sLatestToken = token;
                    }

                    @Override
                    public void onError(@Error int error) {
                        Log.e(TAG, "Failed to clear token. Error: ", error);
                    }
                })
                .clearAndFetch(sLatestToken);
    }

    private static native void nativeResolveOAuthTokenCallback(
            long callbackPtr, int status, String userEmail, String accessToken);
}
