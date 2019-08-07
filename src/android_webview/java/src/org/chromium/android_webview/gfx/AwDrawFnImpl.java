// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.gfx;

import android.graphics.Canvas;

import org.chromium.base.annotations.JNINamespace;

/**
 * Implementation of draw_fn.h.
 */
@JNINamespace("android_webview")
public class AwDrawFnImpl implements AwFunctor {
    private long mNativeAwDrawFnImpl;
    private final DrawFnAccess mAccess;
    private final int mHandle;

    /** Interface for inserting functor into canvas */
    public interface DrawFnAccess { void drawWebViewFunctor(Canvas canvas, int functor); }

    public AwDrawFnImpl(DrawFnAccess access) {
        mAccess = access;
        mNativeAwDrawFnImpl = nativeCreate();
        mHandle = nativeGetFunctorHandle(mNativeAwDrawFnImpl);
    }

    @Override
    public void destroy() {
        assert mNativeAwDrawFnImpl != 0;
        nativeReleaseHandle(mNativeAwDrawFnImpl);
        // Native side is free to destroy itself after ReleaseHandle.
        mNativeAwDrawFnImpl = 0;
    }

    public static void setDrawFnFunctionTable(long functionTablePointer) {
        nativeSetDrawFnFunctionTable(functionTablePointer);
    }

    @Override
    public long getNativeCompositorFrameConsumer() {
        assert mNativeAwDrawFnImpl != 0;
        return nativeGetCompositorFrameConsumer(mNativeAwDrawFnImpl);
    }

    @Override
    public boolean requestDraw(Canvas canvas) {
        assert mNativeAwDrawFnImpl != 0;
        mAccess.drawWebViewFunctor(canvas, mHandle);
        return true;
    }

    @Override
    public void trimMemory() {}

    private native int nativeGetFunctorHandle(long nativeAwDrawFnImpl);
    private native long nativeGetCompositorFrameConsumer(long nativeAwDrawFnImpl);
    private native void nativeReleaseHandle(long nativeAwDrawFnImpl);

    private static native void nativeSetDrawFnFunctionTable(long functionTablePointer);
    private static native long nativeCreate();
}
