// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.base;

import android.app.Activity;
import android.os.AsyncTask;

import com.google.android.gms.auth.GoogleAuthException;
import com.google.android.gms.auth.GoogleAuthUtil;
import com.google.android.gms.auth.UserRecoverableAuthException;

import java.io.IOException;

/**
 * This helper class fetches an OAuth token on a separate thread, and properly handles the various
 * error-conditions that can occur (such as, starting an Activity to prompt user for input).
 */
public class OAuthTokenFetcher {
    /**
     * Callback interface to receive the token, or an error notification. These will be called
     * on the application's main thread. Note that if a user-recoverable error occurs, neither of
     * these callback will be triggered. Instead, a new Activity will be launched, and the calling
     * Activity must override
     * {@link android.app.Activity#onActivityResult} and handle the result code
     * {@link REQUEST_CODE_RECOVER_FROM_OAUTH_ERROR} to re-attempt or cancel fetching the token.
     */
    public interface Callback {
        /** Called when a token is obtained. */
        void onTokenFetched(String token);

        /**
         * Called if an unrecoverable error prevents fetching a token.
         */
        void onError(Error error);
    }

    /** Error types that can be returned as non-recoverable errors from the token-fetcher. */
    public enum Error { NETWORK, UNEXPECTED }

    /** Request code used for starting the OAuth recovery activity. */
    public static final int REQUEST_CODE_RECOVER_FROM_OAUTH_ERROR = 100;

    /**
     * Reference to the main activity. Used for running tasks on the main thread, and for
     * starting other activities to handle user-recoverable errors.
     */
    private Activity mActivity;

    /** Account name (e-mail) for which the token will be fetched. */
    private String mAccountName;

    /** OAuth scope used for the token request. */
    private String mTokenScope;

    private Callback mCallback;

    public OAuthTokenFetcher(Activity activity, String accountName, String tokenScope,
            Callback callback) {
        mActivity = activity;
        mAccountName = accountName;
        mTokenScope = tokenScope;
        mCallback = callback;
    }

    /** Begins fetching a token. Should be called on the main thread. */
    public void fetch() {
        fetchImpl(null);
    }

    /**
     * Begins fetching a token, clearing an existing token from the cache. Should be called on the
     * main thread.
     * @param expiredToken A previously-fetched token which has expired.
     */
    public void clearAndFetch(String expiredToken) {
        fetchImpl(expiredToken);
    }

    private void fetchImpl(final String expiredToken) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                try {
                    if (expiredToken != null) {
                        GoogleAuthUtil.clearToken(mActivity, expiredToken);
                    }

                    // This method is deprecated but its replacement is not yet available.
                    // TODO(lambroslambrou): Fix this by replacing |mAccountName| with an instance
                    // of android.accounts.Account.
                    String token = GoogleAuthUtil.getToken(mActivity, mAccountName, mTokenScope);
                    handleTokenReceived(token);
                } catch (IOException ioException) {
                    handleError(Error.NETWORK);
                } catch (UserRecoverableAuthException recoverableException) {
                    handleRecoverableException(recoverableException);
                } catch (GoogleAuthException fatalException) {
                    handleError(Error.UNEXPECTED);
                }
                return null;
            }
        }.execute();
    }

    private void handleTokenReceived(final String token) {
        mActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mCallback.onTokenFetched(token);
            }
        });
    }

    private void handleError(final Error error) {
        mActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mCallback.onError(error);
            }
        });
    }

    private void handleRecoverableException(final UserRecoverableAuthException exception) {
        mActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mActivity.startActivityForResult(exception.getIntent(),
                        REQUEST_CODE_RECOVER_FROM_OAUTH_ERROR);
            }
        });
    }
}
