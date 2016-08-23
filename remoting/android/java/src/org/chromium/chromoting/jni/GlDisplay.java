// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.jni;

import android.view.Surface;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chromoting.Desktop;
import org.chromium.chromoting.DesktopView;
import org.chromium.chromoting.DesktopViewFactory;
import org.chromium.chromoting.Event;
import org.chromium.chromoting.GlDesktopView;
import org.chromium.chromoting.SizeChangedEventParameter;

/**
 * This class is the JNI interface class that helps bridging GlDesktopView with the OpenGL renderer
 * in native code. The lifetime of this class is managed by the native JniGlDisplayHandler.
 *
 * This class works entirely on the UI thread:
 *  Functions should all be called on UI.
 *  Events will only be triggered on UI.
 */
@JNINamespace("remoting")
public class GlDisplay {
    private volatile long mNativeJniGlDisplay;
    private final Event.Raisable<SizeChangedEventParameter> mOnHostSizeChanged =
            new Event.Raisable<>();
    private final Event.Raisable<Void> mOnCanvasRendered =
            new Event.Raisable<>();

    private GlDisplay(long nativeJniGlDisplay) {
        mNativeJniGlDisplay = nativeJniGlDisplay;
    }

    /**
     * Invalidates this object and disconnects from the native display handler. Called on the
     * display thread by the native code.
     */
    @CalledByNative
    private void invalidate() {
        mNativeJniGlDisplay = 0;
    }

    /**
     * Notifies the OpenGL renderer that a surface for OpenGL to draw is created.
     * @param surface the surface to be drawn on
     */
    public void surfaceCreated(Surface surface) {
        if (mNativeJniGlDisplay != 0) {
            nativeOnSurfaceCreated(mNativeJniGlDisplay, surface);
        }
    }

    /**
     * Notifies the OpenGL renderer the size of the surface. Should be called after surfaceCreated()
     * and before surfaceDestroyed().
     * @param width the width of the surface
     * @param height the height of the surface
     */
    public void surfaceChanged(int width, int height) {
        if (mNativeJniGlDisplay != 0) {
            nativeOnSurfaceChanged(mNativeJniGlDisplay, width, height);
        }
    }

    /**
     * Notifies the OpenGL renderer that the current surface being used is about to be destroyed.
     */
    public void surfaceDestroyed() {
        if (mNativeJniGlDisplay != 0) {
            nativeOnSurfaceDestroyed(mNativeJniGlDisplay);
        }
    }

    /**
     * Sets the transformation matrix (in pixel coordinates).
     * @param matrix the transformation matrix
     */
    public void pixelTransformationChanged(float[] matrix) {
        if (mNativeJniGlDisplay != 0) {
            nativeOnPixelTransformationChanged(mNativeJniGlDisplay, matrix);
        }
    }

    /** Moves the cursor to the corresponding location on the desktop. */
    public void cursorPixelPositionChanged(float x, float y) {
        if (mNativeJniGlDisplay != 0) {
            nativeOnCursorPixelPositionChanged(mNativeJniGlDisplay, x, y);
        }
    }

    /**
     * Decides whether the cursor should be shown on the canvas.
     */
    public void cursorVisibilityChanged(boolean visible) {
        if (mNativeJniGlDisplay != 0) {
            nativeOnCursorVisibilityChanged(mNativeJniGlDisplay, visible);
        }
    }

    /**
     * Called by native code to notify GlDisplay that the size of the canvas (=size of desktop) has
     * changed.
     * @param width width of the canvas
     * @param height height of the canvas
     */
    @CalledByNative
    private void changeCanvasSize(int width, int height) {
        mOnHostSizeChanged.raise(new SizeChangedEventParameter(width, height));
    }

    /**
     * An {@link Event} triggered when the size of the host desktop is changed.
     */
    public Event<SizeChangedEventParameter> onHostSizeChanged() {
        return mOnHostSizeChanged;
    }

    /**
     * Called by native code when a render request has been done by the OpenGL renderer. This
     * will only be called when the render event callback is enabled.
     */
    @CalledByNative
    private void canvasRendered() {
        mOnCanvasRendered.raise(null);
    }

    /**
     * An {@link} triggered when render event callback is enabled and a render request has been done
     * by the OpenGL renderer.
     */
    public Event<Void> onCanvasRendered() {
        return mOnCanvasRendered;
    }

    /**
     * Shows the cursor input feedback animation with the given diameter at the given desktop
     * location.
     */
    public void showCursorInputFeedback(float x, float y, float diameter) {
        if (mNativeJniGlDisplay != 0) {
            nativeOnCursorInputFeedback(mNativeJniGlDisplay, x, y, diameter);
        }
    }

    @CalledByNative
    private void initializeClient(Client client) {
        client.setDesktopViewFactory(new DesktopViewFactory() {
            @Override
            public DesktopView createDesktopView(Desktop desktop, Client client) {
                return new GlDesktopView(GlDisplay.this, desktop, client);
            }
        });
    }

    @CalledByNative
    private static GlDisplay createJavaDisplayObject(long nativeDisplayHandler) {
        return new GlDisplay(nativeDisplayHandler);
    }

    private native void nativeOnSurfaceCreated(long nativeJniGlDisplayHandler, Surface surface);
    private native void nativeOnSurfaceChanged(long nativeJniGlDisplayHandler,
                                               int width, int height);
    private native void nativeOnSurfaceDestroyed(long nativeJniGlDisplayHandler);
    private native void nativeOnPixelTransformationChanged(long nativeJniGlDisplayHandler,
                                                           float[] matrix);
    private native void nativeOnCursorPixelPositionChanged(long nativeJniGlDisplayHandler,
                                                           float x, float y);
    private native void nativeOnCursorInputFeedback(long nativeJniGlDisplayHandler,
                                                    float x, float y, float diameter);
    private native void nativeOnCursorVisibilityChanged(long nativeJniGlDisplayHandler,
                                                        boolean visible);
}
