// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.framehost.RenderFrameHostImpl;
import org.chromium.content_public.browser.RenderFrameHost;

/**
 * The Java wrapper around RenderFrameHost to define test-only operations.
 */
@JNINamespace("content")
public class RenderFrameHostTestExt {
    private final long mNativeRenderFrameHostTestExt;

    public RenderFrameHostTestExt(RenderFrameHost host) {
        mNativeRenderFrameHostTestExt = nativeInit(((RenderFrameHostImpl) host).getNativePtr());
    }

    /**
     * Runs the given JavaScript in the RenderFrameHost.
     *
     * @param script A String containing the JavaScript to run.
     * @param callback The Callback that will be called with the result of the JavaScript execution
     *        serialized to a String using JSONStringValueSerializer.
     */
    public void executeJavaScript(String script, Callback<String> callback) {
        nativeExecuteJavaScript(mNativeRenderFrameHostTestExt, script, callback);
    }

    private native long nativeInit(long renderFrameHostAndroidPtr);
    private native void nativeExecuteJavaScript(
            long nativeRenderFrameHostTestExt, String script, Callback<String> callback);
}
