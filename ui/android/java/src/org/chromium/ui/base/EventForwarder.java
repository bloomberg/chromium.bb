// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.MotionEvent;

import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Class used to forward view, input events down to native.
 */
@JNINamespace("ui")
public class EventForwarder {
    private long mNativeEventForwarder;

    // Offsets for the events that passes through.
    private float mCurrentTouchOffsetX;
    private float mCurrentTouchOffsetY;

    @CalledByNative
    private static EventForwarder create(long nativeEventForwarder) {
        return new EventForwarder(nativeEventForwarder);
    }

    private EventForwarder(long nativeEventForwarder) {
        mNativeEventForwarder = nativeEventForwarder;
    }

    @CalledByNative
    private void destroy() {
        mNativeEventForwarder = 0;
    }

    /**
     * @see View#onTouchEvent(MotionEvent)
     */
    public boolean onTouchEvent(MotionEvent event) {
        // TODO(mustaq): Should we include MotionEvent.TOOL_TYPE_STYLUS here?
        // crbug.com/592082
        if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            // Mouse button info is incomplete on L and below
            int apiVersion = Build.VERSION.SDK_INT;
            if (apiVersion >= android.os.Build.VERSION_CODES.M) {
                return onMouseEvent(event);
            }
        }

        final boolean isTouchHandleEvent = false;
        return sendTouchEvent(event, isTouchHandleEvent);
    }

    /**
     * Called by PopupWindow-based touch handles.
     * @param event the MotionEvent targeting the handle.
     */
    public boolean onTouchHandleEvent(MotionEvent event) {
        final boolean isTouchHandleEvent = true;
        return sendTouchEvent(event, isTouchHandleEvent);
    }

    private boolean sendTouchEvent(MotionEvent event, boolean isTouchHandleEvent) {
        assert mNativeEventForwarder != 0;

        TraceEvent.begin("sendTouchEvent");
        try {
            int eventAction = event.getActionMasked();

            eventAction = SPenSupport.convertSPenEventAction(eventAction);

            if (!isValidTouchEventActionForNative(eventAction)) return false;

            // A zero offset is quite common, in which case the unnecessary copy should be avoided.
            MotionEvent offset = null;
            if (mCurrentTouchOffsetX != 0 || mCurrentTouchOffsetY != 0) {
                offset = createOffsetMotionEvent(event);
                event = offset;
            }

            final int pointerCount = event.getPointerCount();

            float[] touchMajor = {
                    event.getTouchMajor(), pointerCount > 1 ? event.getTouchMajor(1) : 0};
            float[] touchMinor = {
                    event.getTouchMinor(), pointerCount > 1 ? event.getTouchMinor(1) : 0};

            for (int i = 0; i < 2; i++) {
                if (touchMajor[i] < touchMinor[i]) {
                    float tmp = touchMajor[i];
                    touchMajor[i] = touchMinor[i];
                    touchMinor[i] = tmp;
                }
            }

            final boolean consumed = nativeOnTouchEvent(mNativeEventForwarder, event,
                    event.getEventTime(), eventAction, pointerCount, event.getHistorySize(),
                    event.getActionIndex(), event.getX(), event.getY(),
                    pointerCount > 1 ? event.getX(1) : 0, pointerCount > 1 ? event.getY(1) : 0,
                    event.getPointerId(0), pointerCount > 1 ? event.getPointerId(1) : -1,
                    touchMajor[0], touchMajor[1], touchMinor[0], touchMinor[1],
                    event.getOrientation(), pointerCount > 1 ? event.getOrientation(1) : 0,
                    event.getAxisValue(MotionEvent.AXIS_TILT),
                    pointerCount > 1 ? event.getAxisValue(MotionEvent.AXIS_TILT, 1) : 0,
                    event.getRawX(), event.getRawY(), event.getToolType(0),
                    pointerCount > 1 ? event.getToolType(1) : MotionEvent.TOOL_TYPE_UNKNOWN,
                    event.getButtonState(), event.getMetaState(), isTouchHandleEvent);

            if (offset != null) offset.recycle();
            return consumed;
        } finally {
            TraceEvent.end("sendTouchEvent");
        }
    }

    /**
     * Sets the current amount to offset incoming touch events by (including MotionEvent and
     * DragEvent). This is used to handle content moving and not lining up properly with the
     * android input system.
     * @param dx The X offset in pixels to shift touch events.
     * @param dy The Y offset in pixels to shift touch events.
     */
    public void setCurrentTouchEventOffsets(float dx, float dy) {
        mCurrentTouchOffsetX = dx;
        mCurrentTouchOffsetY = dy;
    }

    private MotionEvent createOffsetMotionEvent(MotionEvent src) {
        MotionEvent dst = MotionEvent.obtain(src);
        dst.offsetLocation(mCurrentTouchOffsetX, mCurrentTouchOffsetY);
        return dst;
    }

    private static boolean isValidTouchEventActionForNative(int eventAction) {
        // Only these actions have any effect on gesture detection.  Other
        // actions have no corresponding WebTouchEvent type and may confuse the
        // touch pipline, so we ignore them entirely.
        return eventAction == MotionEvent.ACTION_DOWN || eventAction == MotionEvent.ACTION_UP
                || eventAction == MotionEvent.ACTION_CANCEL
                || eventAction == MotionEvent.ACTION_MOVE
                || eventAction == MotionEvent.ACTION_POINTER_DOWN
                || eventAction == MotionEvent.ACTION_POINTER_UP;
    }

    @TargetApi(Build.VERSION_CODES.M)
    public boolean onMouseEvent(MotionEvent event) {
        TraceEvent.begin("sendMouseEvent");

        MotionEvent offsetEvent = createOffsetMotionEvent(event);
        try {
            int eventAction = event.getActionMasked();

            // For mousedown and mouseup events, we use ACTION_BUTTON_PRESS
            // and ACTION_BUTTON_RELEASE respectively because they provide
            // info about the changed-button.
            if (eventAction == MotionEvent.ACTION_DOWN || eventAction == MotionEvent.ACTION_UP) {
                return false;
            }
            sendMouseEvent(event.getEventTime(), eventAction, offsetEvent.getX(),
                    offsetEvent.getY(), event.getPointerId(0), event.getPressure(0),
                    event.getOrientation(0), event.getAxisValue(MotionEvent.AXIS_TILT, 0),
                    event.getActionButton(), event.getButtonState(), event.getMetaState(),
                    event.getToolType(0));
            return true;
        } finally {
            offsetEvent.recycle();
            TraceEvent.end("sendMouseEvent");
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    public void sendMouseEvent(long timeMs, int action, float x, float y, int pointerId,
            float pressure, float orientation, float tilt, int actionButton, int buttonState,
            int metaState, int toolType) {
        assert mNativeEventForwarder != 0;
        nativeOnMouseEvent(mNativeEventForwarder, timeMs, action, x, y, pointerId, pressure,
                orientation, tilt, actionButton, buttonState, metaState, toolType);
    }

    // All touch events (including flings, scrolls etc) accept coordinates in physical pixels.
    private native boolean nativeOnTouchEvent(long nativeEventForwarder, MotionEvent event,
            long timeMs, int action, int pointerCount, int historySize, int actionIndex, float x0,
            float y0, float x1, float y1, int pointerId0, int pointerId1, float touchMajor0,
            float touchMajor1, float touchMinor0, float touchMinor1, float orientation0,
            float orientation1, float tilt0, float tilt1, float rawX, float rawY,
            int androidToolType0, int androidToolType1, int androidButtonState,
            int androidMetaState, boolean isTouchHandleEvent);
    private native void nativeOnMouseEvent(long nativeEventForwarder, long timeMs, int action,
            float x, float y, int pointerId, float pressure, float orientation, float tilt,
            int changedButton, int buttonState, int metaState, int toolType);
}
