// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.host.jni;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Class to allow Java code to access the native C++ implementation of the Host process. This class
 * controls the lifetime of the corresponding C++ object.
 */
@JNINamespace("remoting")
public class Host {
    private static final String TAG = "host";

    // Pointer to the C++ object, cast to a |long|.
    private long mNativeJniHost;

    private It2MeHostObserver mObserver;

    /**
     * To be called once from the Application context singleton. Loads and initializes the native
     * code. Called on the UI thread.
     * @param context The Application context.
     */
    public static void loadLibrary(Context context) {
        ContextUtils.initApplicationContext(context.getApplicationContext());
        System.loadLibrary("remoting_host_jni");
        ContextUtils.initApplicationContextForNative();
    }

    public Host() {
        mNativeJniHost = nativeInit();
    }

    private native long nativeInit();

    public void destroy() {
        nativeDestroy(mNativeJniHost);
        mNativeJniHost = 0;
    }

    private native void nativeDestroy(long nativeJniHost);

    public void connect(String userName, String authToken, It2MeHostObserver observer) {
        mObserver = observer;
        nativeConnect(mNativeJniHost, userName, authToken);
    }

    private native void nativeConnect(long nativeJniHost, String userName, String authToken);

    public void disconnect() {
        nativeDisconnect(mNativeJniHost);
    }

    private native void nativeDisconnect(long nativeJniHost);

    @CalledByNative
    private void onStateChanged(int state, String errorMessage) {
        It2MeHostObserver.State[] states = It2MeHostObserver.State.values();
        if (state < 0 || state >= states.length) {
            Log.e(TAG, "Invalid It2Me state: %d", state);
            return;
        }
        mObserver.onStateChanged(states[state], errorMessage);
    }

    @CalledByNative
    private void onAccessCodeReceived(String accessCode, int lifetimeSeconds) {
        mObserver.onAccessCodeReceived(accessCode, lifetimeSeconds);
    }
}
