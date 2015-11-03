// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.File;

/**
 * Java bindings for the native embedded test server.
 *
 * An example use:
 *   EmbeddedTestServer s = new EmbeddedTestServer();
 *   s.initializeNative();
 *   s.serveFilesFromDirectory("/path/to/my/directory");
 *   if (!s.start()) {
 *       throw new SomeKindOfException("Unable to initialize EmbeddedTestServer.");
 *   }
 *
 *   // serve requests...
 *
 *   s.shutdownAndWait();
 *   s.destroy();
 */
@JNINamespace("net::test_server")
public class EmbeddedTestServer {
    private long mNativeEmbeddedTestServer;

    /** Create an uninitialized EmbeddedTestServer. */
    public EmbeddedTestServer() {}

    /** Initialize the native EmbeddedTestServer object. */
    public void initializeNative() {
        if (mNativeEmbeddedTestServer == 0) nativeInit();
        assert mNativeEmbeddedTestServer != 0;
    }

    /** Serve files from the provided directory.
     *
     *  @param directory The directory from which files should be served.
     */
    public void serveFilesFromDirectory(File directory) {
        nativeServeFilesFromDirectory(mNativeEmbeddedTestServer, directory.getPath());
    }

    /** Serve files from the provided directory.
     *
     *  @param directoryPath The path of the directory from which files should be served.
     */
    public void serveFilesFromDirectory(String directoryPath) {
        nativeServeFilesFromDirectory(mNativeEmbeddedTestServer, directoryPath);
    }

    // TODO(svaldez): Remove once all consumers have switched to start().
    /** Wrapper for start()
     *
     *  start() should be used instead of this.
     *
     *  @return Whether the server was successfully initialized.
     */
    public boolean initializeAndWaitUntilReady() {
        return start();
    }

    /** Starts the server.
     *
     *  Note that this should be called after handlers are set up, including any relevant calls
     *  serveFilesFromDirectory.
     *
     *  @return Whether the server was successfully initialized.
     */
    public boolean start() {
        return nativeStart(mNativeEmbeddedTestServer);
    }

    /** Get the full URL for the given relative URL.
     *
     *  @param relativeUrl The relative URL for which a full URL will be obtained.
     *  @return The URL as a String.
     */
    public String getURL(String relativeUrl) {
        return nativeGetURL(mNativeEmbeddedTestServer, relativeUrl);
    }

    /** Shutdown the server.
     *
     *  @return Whether the server was successfully shut down.
     */
    public boolean shutdownAndWaitUntilComplete() {
        return nativeShutdownAndWaitUntilComplete(mNativeEmbeddedTestServer);
    }

    /** Destroy the native EmbeddedTestServer object. */
    public void destroy() {
        assert mNativeEmbeddedTestServer != 0;
        nativeDestroy(mNativeEmbeddedTestServer);
        assert mNativeEmbeddedTestServer == 0;
    }

    @CalledByNative
    private void setNativePtr(long nativePtr) {
        assert mNativeEmbeddedTestServer == 0;
        mNativeEmbeddedTestServer = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        assert mNativeEmbeddedTestServer != 0;
        mNativeEmbeddedTestServer = 0;
    }

    private native void nativeInit();
    private native void nativeDestroy(long nativeEmbeddedTestServerAndroid);
    private native boolean nativeStart(long nativeEmbeddedTestServerAndroid);
    private native boolean nativeShutdownAndWaitUntilComplete(long nativeEmbeddedTestServerAndroid);
    private native String nativeGetURL(long nativeEmbeddedTestServerAndroid, String relativeUrl);
    private native void nativeServeFilesFromDirectory(
            long nativeEmbeddedTestServerAndroid, String directoryPath);
}
