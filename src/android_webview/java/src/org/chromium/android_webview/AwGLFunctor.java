// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Canvas;
import android.view.ViewGroup;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Manages state associated with the Android render thread and the draw functor
 * that the WebView uses to render its contents. AwGLFunctor is responsible for
 * managing the lifetime of native RenderThreadManager and HardwareRenderer,
 * ensuring that they continue to exist while the functor remains attached to
 * the render node hierarchy.
 */
@JNINamespace("android_webview")
public class AwGLFunctor implements AwFunctor {
    private final long mNativeAwGLFunctor;
    private final AwContents.NativeDrawGLFunctor mNativeDrawGLFunctor;
    private final ViewGroup mContainerView;
    private final Runnable mFunctorReleasedCallback;
    // Counts outstanding requestDrawGL calls as well as window attach count.
    private int mRefCount;

    public AwGLFunctor(
            AwContents.NativeDrawFunctorFactory nativeDrawFunctorFactory, ViewGroup containerView) {
        mNativeAwGLFunctor = nativeCreate(this);
        mNativeDrawGLFunctor = nativeDrawFunctorFactory.createGLFunctor(
                nativeGetAwDrawGLViewContext(mNativeAwGLFunctor));
        mContainerView = containerView;
        if (mNativeDrawGLFunctor.supportsDrawGLFunctorReleasedCallback()) {
            mFunctorReleasedCallback = () -> removeReference();
        } else {
            mFunctorReleasedCallback = null;
        }
        addReference();
    }

    @Override
    public void destroy() {
        assert mRefCount > 0;
        removeReference();
    }

    public static long getAwDrawGLFunction() {
        return nativeGetAwDrawGLFunction();
    }

    @Override
    public long getNativeCompositorFrameConsumer() {
        assert mRefCount > 0;
        return nativeGetCompositorFrameConsumer(mNativeAwGLFunctor);
    }

    @Override
    public boolean requestDraw(Canvas canvas) {
        assert mRefCount > 0;
        boolean success = mNativeDrawGLFunctor.requestDrawGL(canvas, mFunctorReleasedCallback);
        if (success && mFunctorReleasedCallback != null) {
            addReference();
        }
        return success;
    }

    private void addReference() {
        ++mRefCount;
    }

    private void removeReference() {
        assert mRefCount > 0;
        if (--mRefCount == 0) {
            // When |mRefCount| decreases to zero, the functor is neither attached to a view, nor
            // referenced from the render tree, and so it is safe to delete the HardwareRenderer
            // instance to free up resources because the current state will not be drawn again.
            nativeDeleteHardwareRenderer(mNativeAwGLFunctor);
            mNativeDrawGLFunctor.destroy();
            nativeDestroy(mNativeAwGLFunctor);
        }
    }

    @CalledByNative
    private boolean requestInvokeGL(boolean waitForCompletion) {
        return mNativeDrawGLFunctor.requestInvokeGL(mContainerView, waitForCompletion);
    }

    @CalledByNative
    private void detachFunctorFromView() {
        mNativeDrawGLFunctor.detach(mContainerView);
        mContainerView.invalidate();
    }

    @Override
    public void trimMemory() {
        assert mRefCount > 0;
        nativeDeleteHardwareRenderer(mNativeAwGLFunctor);
    }

    /**
     * Intended for test code.
     * @return the number of native instances of this class.
     */
    @VisibleForTesting
    public static int getNativeInstanceCount() {
        return nativeGetNativeInstanceCount();
    }

    private native void nativeDeleteHardwareRenderer(long nativeAwGLFunctor);
    private native long nativeGetAwDrawGLViewContext(long nativeAwGLFunctor);
    private native long nativeGetCompositorFrameConsumer(long nativeAwGLFunctor);

    private static native long nativeGetAwDrawGLFunction();
    private static native void nativeDestroy(long nativeAwGLFunctor);
    private static native long nativeCreate(AwGLFunctor javaProxy);
    private static native int nativeGetNativeInstanceCount();
}
