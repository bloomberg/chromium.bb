// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.host.jni;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;

/**
 * Class to allow Java code to access the native C++ implementation of the Host process. This class
 * controls the lifetime of the corresponding C++ object.
 */
@JNINamespace("remoting")
public class Host {
    // Pointer to the C++ object, cast to a |long|.
    private long mNativeJniHost;

    /**
     * To be called once from the Application context singleton. Loads and initializes the native
     * code. Called on the UI thread.
     * @param context The Application context.
     */
    public static void loadLibrary(Context context) {
        System.loadLibrary("remoting_host_jni");
        ContextUtils.initApplicationContext(context);
    }

    public Host() {
        mNativeJniHost = nativeInit();
    }

    private native long nativeInit();

    public void destroy() {
        nativeDestroy(mNativeJniHost);
    }

    private native void nativeDestroy(long nativeJniHost);

    public void connect(String userName, String authToken) {
        nativeConnect(mNativeJniHost, userName, authToken);
    }

    private native void nativeConnect(long nativeJniHost, String userName, String authToken);

    public void disconnect() {
        nativeDisconnect(mNativeJniHost);
    }

    private native void nativeDisconnect(long nativeJniHost);
}
