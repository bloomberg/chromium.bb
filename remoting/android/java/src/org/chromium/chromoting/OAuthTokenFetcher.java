// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

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
         * @param errorResource String resource of error-message to be displayed.
         */
        void onError(int errorResource);
    }

    /** Request code used for starting the OAuth recovery activity. */
    public static final int REQUEST_CODE_RECOVER_FROM_OAUTH_ERROR = 100;

    /** Scopes at which the authentication token we request will be valid. */
    private static final String TOKEN_SCOPE = "oauth2:https://www.googleapis.com/auth/chromoting "
            + "https://www.googleapis.com/auth/googletalk";

    /**
     * Reference to the main activity. Used for running tasks on the main thread, and for
     * starting other activities to handle user-recoverable errors.
     */
    private Activity mActivity;

    /** Account name (e-mail) for which the token will be fetched. */
    private String mAccountName;

    private Callback mCallback;

    public OAuthTokenFetcher(Activity activity, String accountName, Callback callback) {
        mActivity = activity;
        mAccountName = accountName;
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
                    String token = GoogleAuthUtil.getToken(mActivity, mAccountName, TOKEN_SCOPE);
                    handleTokenReceived(token);
                } catch (IOException ioException) {
                    handleError(R.string.error_network_error);
                } catch (UserRecoverableAuthException recoverableException) {
                    handleRecoverableException(recoverableException);
                } catch (GoogleAuthException fatalException) {
                    handleError(R.string.error_unexpected);
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

    private void handleError(final int error) {
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
