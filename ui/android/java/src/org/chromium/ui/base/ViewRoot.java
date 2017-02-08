// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.view.MotionEvent;

import org.chromium.base.annotations.JNINamespace;

/**
 * Class used to forward view, input events down to native.
 * TODO(jinsukkim): Hook this up to UI event forwarding flow and substitute WindowAndroid.
 */
@JNINamespace("ui")
public class ViewRoot {

    private final WindowAndroid mWindowAndroid;

    // The corresponding native instance. This class can only be used while
    // the native instance is alive.
    // This is initialized lazily. Use {@link getNativePtr()} rather than
    // accessing it directly.
    private long mNativeView;

    public static ViewRoot create(WindowAndroid window) {
        if (window == null) throw new IllegalArgumentException("WindowAndroid should not be null");
        return new ViewRoot(window);
    }

    private ViewRoot(WindowAndroid window) {
        mWindowAndroid = window;
    }

    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    public boolean onTouchEvent(MotionEvent event, boolean isTouchHandleEvent) {
        final int pointerCount = event.getPointerCount();

        float[] touchMajor = {event.getTouchMajor(),
                              pointerCount > 1 ? event.getTouchMajor(1) : 0};
        float[] touchMinor = {event.getTouchMinor(),
                              pointerCount > 1 ? event.getTouchMinor(1) : 0};

        for (int i = 0; i < 2; i++) {
            if (touchMajor[i] < touchMinor[i]) {
                float tmp = touchMajor[i];
                touchMajor[i] = touchMinor[i];
                touchMinor[i] = tmp;
            }
        }

        return nativeOnTouchEvent(getNativePtr(), event,
                event.getEventTime(), event.getActionMasked(),
                pointerCount, event.getHistorySize(), event.getActionIndex(),
                event.getX(), event.getY(),
                pointerCount > 1 ? event.getX(1) : 0,
                pointerCount > 1 ? event.getY(1) : 0,
                event.getPointerId(0), pointerCount > 1 ? event.getPointerId(1) : -1,
                touchMajor[0], touchMajor[1],
                touchMinor[0], touchMinor[1],
                event.getOrientation(), pointerCount > 1 ? event.getOrientation(1) : 0,
                event.getAxisValue(MotionEvent.AXIS_TILT),
                pointerCount > 1 ? event.getAxisValue(MotionEvent.AXIS_TILT, 1) : 0,
                event.getRawX(), event.getRawY(),
                event.getToolType(0),
                pointerCount > 1 ? event.getToolType(1) : MotionEvent.TOOL_TYPE_UNKNOWN,
                event.getButtonState(),
                event.getMetaState(),
                isTouchHandleEvent);
    }

    public long getNativePtr() {
        if (mNativeView == 0) mNativeView = nativeInit(mWindowAndroid.getNativePointer());
        return mNativeView;
    }

    public void destroy() {
        if (mNativeView != 0) {
            nativeDestroy(mNativeView);
            mNativeView = 0;
        }
    }

    private native long nativeInit(long windowNativePointer);
    private native void nativeDestroy(long nativeViewRoot);

    // All touch events (including flings, scrolls etc) accept coordinates in physical pixels.
    private native boolean nativeOnTouchEvent(
            long nativeViewRoot, MotionEvent event,
            long timeMs, int action, int pointerCount, int historySize, int actionIndex,
            float x0, float y0, float x1, float y1,
            int pointerId0, int pointerId1,
            float touchMajor0, float touchMajor1,
            float touchMinor0, float touchMinor1,
            float orientation0, float orientation1,
            float tilt0, float tilt1,
            float rawX, float rawY,
            int androidToolType0, int androidToolType1,
            int androidButtonState, int androidMetaState,
            boolean isTouchHandleEvent);
}
