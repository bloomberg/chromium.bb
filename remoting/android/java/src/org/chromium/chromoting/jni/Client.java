// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import org.chromium.base.annotations.JNINamespace;

/**
 * Class to manage a client connection to the host. This class controls the lifetime of the
 * corresponding C++ object which implements the connection. A new object should be created for
 * each connection to the host, so that notifications from a connection are always sent to the
 * right object.
 */
@JNINamespace("remoting")
public class Client {
    // Pointer to the C++ object, cast to a |long|.
    private long mNativeJniClient;

    public void init() {
        mNativeJniClient = nativeInit();
    }

    private native long nativeInit();

    public void destroy() {
        nativeDestroy(mNativeJniClient);
    }

    private native void nativeDestroy(long nativeJniClient);
}
