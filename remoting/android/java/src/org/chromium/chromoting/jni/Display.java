// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.graphics.Bitmap;
import android.graphics.Point;
import android.os.Looper;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * This class is for drawing the desktop on the canvas and controlling the lifetime of the
 * corresponding C++ object. It only draws the desktop on the graphics (=display) thread but also
 * has functions accessible on UI thread.
 */
@JNINamespace("remoting")
public class Display {
    private static final String TAG = "Chromoting";

    // Pointer to the C++ object. Casted to |long|.
    private long mNativeJniDisplayHandler;

    /**
     * Callback invoked on the graphics thread to repaint the desktop. Read on the UI and
     * graphics threads. Write only on the UI thread.
     */
    private Runnable mRedrawCallback;

    /**
     * Protects access to {@link mRedrawCallback}.
     * Locking is not required when reading on the UI thread.
     */
    private final Object mRedrawCallbackLock = new Object();

    /** Protects access to {@link mFrameBitmap}. */
    private final Object mFrameLock = new Object();

    /** Bitmap holding a copy of the latest video frame. Accessed on the UI and graphics threads. */
    private Bitmap mFrameBitmap;

    /** Position of cursor hot-spot. Accessed on the graphics thread. */
    private Point mCursorHotspot = new Point();

    /** Bitmap holding the cursor shape. Accessed on the graphics thread. */
    private Bitmap mCursorBitmap;

    private Display(long nativeDisplayHandler) {
        mNativeJniDisplayHandler = nativeDisplayHandler;
    }

    /**
     * @return the pointer to the native C++ object.
     */
    public long getNativePointer() {
        return mNativeJniDisplayHandler;
    }

    /**
     * Sets the redraw callback to the provided functor. Provide a value of null whenever the
     * window is no longer visible so that we don't continue to draw onto it. Called on the UI
     * thread.
     */
    public void provideRedrawCallback(Runnable redrawCallback) {
        synchronized (mRedrawCallbackLock) {
            mRedrawCallback = redrawCallback;
        }
    }

    /**
     * Invalidates this object and disconnects from the native display handler. Called on the
     * display thread by the native code.
     */
    @CalledByNative
    private void invalidate() {
        // Drop the reference to free the Bitmap for GC.
        synchronized (mFrameLock) {
            mFrameBitmap = null;
        }

        provideRedrawCallback(null);
        mNativeJniDisplayHandler = 0;
    }

    /** Forces the native graphics thread to redraw to the canvas. Called on the UI thread. */
    public boolean redrawGraphics() {
        if (mRedrawCallback == null) return false;

        nativeScheduleRedraw(mNativeJniDisplayHandler);
        return true;
    }

    /**
     * Called on the graphics thread to perform the redrawing callback requested by
     * {@link #redrawGraphics}. This is a no-op if the window isn't visible (the callback is null).
     */
    @CalledByNative
    void redrawGraphicsInternal() {
        Runnable callback;
        synchronized (mRedrawCallbackLock) {
            callback = mRedrawCallback;
        }
        if (callback != null) {
            callback.run();
        }
    }

    /**
     * Returns a bitmap of the latest video frame. Called on the native graphics thread when
     * DesktopView is repainted.
     */
    public Bitmap getVideoFrame() {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w(TAG, "Canvas being redrawn on UI thread");
        }

        synchronized (mFrameLock) {
            return mFrameBitmap;
        }
    }

    /**
     * Set a new video frame. Called on the native graphics thread when a new frame is allocated.
     */
    @CalledByNative
    void setVideoFrame(Bitmap bitmap) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w(TAG, "Video frame updated on UI thread");
        }

        synchronized (mFrameLock) {
            mFrameBitmap = bitmap;
        }
    }

    /**
     * Creates a new Bitmap to hold video frame pixels. The returned Bitmap is referenced by native
     * code which writes the decoded frame pixels to it.
     */
    @CalledByNative
    static Bitmap newBitmap(int width, int height) {
        return Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    }

    /**
     * Updates the cursor shape. This is called on the graphics thread when receiving a new cursor
     * shape from the host.
     */
    @CalledByNative
    void updateCursorShape(int width, int height, int hotspotX, int hotspotY, ByteBuffer buffer) {
        mCursorHotspot = new Point(hotspotX, hotspotY);

        int[] data = new int[width * height];
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        buffer.asIntBuffer().get(data, 0, data.length);
        mCursorBitmap = Bitmap.createBitmap(data, width, height, Bitmap.Config.ARGB_8888);
    }

    /** Position of cursor hotspot within cursor image. Called on the graphics thread. */
    public Point getCursorHotspot() {
        return mCursorHotspot;
    }

    /** Returns the current cursor shape. Called on the graphics thread. */
    public Bitmap getCursorBitmap() {
        return mCursorBitmap;
    }

    @CalledByNative
    private static Display createJavaDisplayObject(long nativeDisplayHandler) {
        return new Display(nativeDisplayHandler);
    }

    /** Schedules a redraw on the native graphics thread. */
    private native void nativeScheduleRedraw(long nativeJniDisplayHandler);
}
