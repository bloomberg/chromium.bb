// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system.impl;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.mojo.system.RunLoop;

/**
 * Implementation of {@link RunLoop} suitable for the base:: message loop implementation.
 */
@JNINamespace("mojo::android")
class BaseRunLoop implements RunLoop {
    /**
     * Pointer to the C run loop.
     */
    private long mRunLoopID;
    private final CoreImpl mCore;

    BaseRunLoop(CoreImpl core) {
        this.mCore = core;
        this.mRunLoopID = nativeCreateBaseRunLoop();
    }

    @Override
    public void run() {
        assert mRunLoopID != 0 : "The run loop cannot run once closed";
        nativeRun();
    }

    @Override
    public void runUntilIdle() {
        assert mRunLoopID != 0 : "The run loop cannot run once closed";
        nativeRunUntilIdle();
    }

    @Override
    public void quit() {
        assert mRunLoopID != 0 : "The run loop cannot be quitted run once closed";
        nativeQuit();
    }

    @Override
    public void postDelayedTask(Runnable runnable, long delay) {
        assert mRunLoopID != 0 : "The run loop cannot run tasks once closed";
        nativePostDelayedTask(mRunLoopID, runnable, delay);
    }

    @Override
    public void close() {
        if (mRunLoopID == 0) {
            return;
        }
        // We don't want to de-register a different run loop!
        assert mCore.getCurrentRunLoop() == this : "Only the current run loop can be closed";
        mCore.clearCurrentRunLoop();
        nativeDeleteMessageLoop(mRunLoopID);
        mRunLoopID = 0;
    }

    @CalledByNative
    private static void runRunnable(Runnable runnable) {
        runnable.run();
    }

    private native long nativeCreateBaseRunLoop();
    private native void nativeRun();
    private native void nativeRunUntilIdle();
    private native void nativeQuit();
    private native void nativePostDelayedTask(long runLoopID, Runnable runnable, long delay);
    private native void nativeDeleteMessageLoop(long runLoopID);
}
