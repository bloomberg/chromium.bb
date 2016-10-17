// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chromoting.OAuthTokenConsumer;
import org.chromium.chromoting.base.OAuthTokenFetcher;

/**
 * Initializes the Chromium remoting library, and provides JNI calls into it.
 * All interaction with the native code is centralized in this class.
 */
@JNINamespace("remoting")
public class JniInterface {
    private static final String TAG = "Chromoting";

    private static final String TOKEN_SCOPE = "oauth2:https://www.googleapis.com/auth/chromoting";

    private static final String LIBRARY_NAME = "remoting_client_jni";

    // Used to fetch auth token for native client.
    private static OAuthTokenConsumer sLoggerTokenConsumer;

    private static String sAccount;

    /**
     * To be called once from the Application context singleton. Loads and initializes the native
     * code. Called on the UI thread.
     * @param context The Application context.
     */
    public static void loadLibrary(Context context) {
        ContextUtils.initApplicationContext(context.getApplicationContext());
        sLoggerTokenConsumer = new OAuthTokenConsumer(context.getApplicationContext(), TOKEN_SCOPE);
        try {
            System.loadLibrary(LIBRARY_NAME);
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "Couldn't load " + LIBRARY_NAME + ", trying " + LIBRARY_NAME + ".cr");
            System.loadLibrary(LIBRARY_NAME + ".cr");
        }
        ContextUtils.initApplicationContextForNative();
        nativeLoadNative();
    }

    public static void setAccountForLogging(String account) {
        sAccount = account;
        fetchAuthToken();
    }

    /**
     * Fetch the OAuth token and feed it to the native interface.
     */
    @CalledByNative
    private static void fetchAuthToken() {
        if (sAccount == null) {
            throw new IllegalStateException("Account is not set before fetching the auth token.");
        }
        sLoggerTokenConsumer.consume(sAccount, new OAuthTokenFetcher.Callback() {
            @Override
            public void onTokenFetched(String token) {
                nativeOnAuthTokenFetched(token);
            }

            @Override
            public void onError(OAuthTokenFetcher.Error error) {
                Log.e(TAG, "Failed to fetch auth token for native client.");
            }
        });
    }

    /** Performs the native portion of the initialization. */
    private static native void nativeLoadNative();

    /** Notifies the native client with the new auth token */
    private static native void nativeOnAuthTokenFetched(String token);
}
