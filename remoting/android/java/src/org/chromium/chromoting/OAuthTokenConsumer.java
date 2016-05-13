// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.Activity;

import org.chromium.chromoting.base.OAuthTokenFetcher;

/**
 * This helper guards same auth token requesting task shouldn't be run more than once at the same
 * time.
 */
public class OAuthTokenConsumer {
    private Activity mActivity;
    private String mTokenScope;
    private boolean mWaitingForAuthToken;
    private String mLatestToken;

    /**
     * @param activity The Chromoting activity.
     * @param tokenScope Scope to use when fetching the OAuth token.
     */
    public OAuthTokenConsumer(Activity activity, String tokenScope) {
        mActivity = activity;
        mTokenScope = tokenScope;
        mWaitingForAuthToken = false;
    }

    /**
     * Retrieves the auth token and call the callback when it is done. callback.onTokenFetched()
     * will be called if the retrieval succeeds, otherwise callback.onError() will be called.
     * The callback will not be run if the task is already running and false will be returned in
     * that case.
     * Each OAuthTokenConsumer is supposed to work for one specific task. It is the caller's
     * responsibility to supply equivalent callbacks (variables being captured can vary) for the
     * same consumer.
     * @param account User's account name (email).
     * @param callback the callback to be called
     * @return true if the consumer will run |callback| when the token is fetched and false
     *          otherwise (meaning a previous callback is waiting to be run).
     */
    public boolean consume(String account, final OAuthTokenFetcher.Callback callback) {
        if (mWaitingForAuthToken) {
            return false;
        }
        mWaitingForAuthToken = true;

        new OAuthTokenFetcher(mActivity, account, mTokenScope, new OAuthTokenFetcher.Callback() {
            @Override
            public void onTokenFetched(String token) {
                mWaitingForAuthToken = false;
                mLatestToken = token;
                callback.onTokenFetched(token);
            }

            @Override
            public void onError(OAuthTokenFetcher.Error error) {
                mWaitingForAuthToken = false;
                callback.onError(error);
            }
        }).fetch();
        return true;
    }

    /**
     * @return The latest auth token fetched by calling consume(). This should be used right after
     *          the callback passed to consume() is run. The token may become invalid after some
     *          amount of time.
     */
    public String getLatestToken() {
        return mLatestToken;
    }
}
