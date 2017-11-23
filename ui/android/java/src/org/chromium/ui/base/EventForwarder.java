// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.annotation.TargetApi;
import android.content.ClipData;
import android.content.ClipDescription;
import android.os.Build;
import android.view.DragEvent;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Class used to forward view, input events down to native.
 */
@JNINamespace("ui")
public class EventForwarder {
    private final boolean mIsDragDropEnabled;

    private long mNativeEventForwarder;

    // Offsets for the events that passes through.
    private float mCurrentTouchOffsetX;
    private float mCurrentTouchOffsetY;

    private int mLastMouseButtonState;

    @CalledByNative
    private static EventForwarder create(long nativeEventForwarder, boolean isDragDropEnabled) {
        return new EventForwarder(nativeEventForwarder, isDragDropEnabled);
    }

    private EventForwarder(long nativeEventForwarder, boolean isDragDropEnabled) {
        mNativeEventForwarder = nativeEventForwarder;
        mIsDragDropEnabled = isDragDropEnabled;
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
            // Skip firing mouse events in the follwoing cases:
            // - In Android L and below, where mouse button info is incomplete.
            // - A move w/o a button press, which represents a trackpad scroll. Real mouse moves w/o
            //   buttons goes to onHoverEvent.
            final int apiVersion = Build.VERSION.SDK_INT;
            final boolean isTouchpadScroll = event.getButtonState() == 0
                    && (event.getActionMasked() == MotionEvent.ACTION_DOWN
                               || event.getActionMasked() == MotionEvent.ACTION_MOVE
                               || event.getActionMasked() == MotionEvent.ACTION_UP);

            if (apiVersion >= android.os.Build.VERSION_CODES.M && !isTouchpadScroll) {
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
            // Android may batch multiple events together for efficiency. We
            // want to use the oldest event time as hardware time stamp.
            final long oldestEventTime = event.getHistorySize() > 0
                    ? event.getHistoricalEventTime(0)
                    : event.getEventTime();

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
                    oldestEventTime, eventAction, pointerCount, event.getHistorySize(),
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

    /**
     * @see View#onHoverEvent(MotionEvent)
     */
    public boolean onHoverEvent(MotionEvent event) {
        TraceEvent.begin("onHoverEvent");
        try {
            return sendNativeMouseEvent(event);
        } finally {
            TraceEvent.end("onHoverEvent");
        }
    }

    /**
     * @see View#onMouseEvent(MotionEvent)
     */
    public boolean onMouseEvent(MotionEvent event) {
        TraceEvent.begin("sendMouseEvent");
        try {
            return sendNativeMouseEvent(event);
        } finally {
            TraceEvent.end("sendMouseEvent");
        }
    }

    /**
     * Sends mouse event to native. Hover event is also converted to mouse event,
     * only differentiated by an internal flag.
     */
    private boolean sendNativeMouseEvent(MotionEvent event) {
        assert mNativeEventForwarder != 0;

        MotionEvent offsetEvent = createOffsetMotionEvent(event);
        try {
            int eventAction = event.getActionMasked();

            if (eventAction == MotionEvent.ACTION_BUTTON_PRESS
                    || eventAction == MotionEvent.ACTION_BUTTON_RELEASE) {
                mLastMouseButtonState = event.getButtonState();
            }
            // Work around Samsung Galaxy Tab 2 not sending ACTION_BUTTON_RELEASE on left-click:
            // http://crbug.com/714230.  On ACTION_HOVER, no button can be pressed, so send a
            // synthetic ACTION_BUTTON_RELEASE if it was missing.  Note that all
            // ACTION_BUTTON_RELEASE are always fired before any hover events on a correctly
            // behaving device, so mLastMouseButtonState is only nonzero on a buggy one.
            if (eventAction == MotionEvent.ACTION_HOVER_ENTER) {
                if (mLastMouseButtonState == MotionEvent.BUTTON_PRIMARY) {
                    nativeOnMouseEvent(mNativeEventForwarder, event.getEventTime(),
                            MotionEvent.ACTION_BUTTON_RELEASE, offsetEvent.getX(),
                            offsetEvent.getY(), event.getPointerId(0), event.getPressure(0),
                            event.getOrientation(0), event.getAxisValue(MotionEvent.AXIS_TILT, 0),
                            MotionEvent.BUTTON_PRIMARY, event.getButtonState(),
                            event.getMetaState(), event.getToolType(0));
                }
                mLastMouseButtonState = 0;
            }

            // Ignore ACTION_HOVER_ENTER & ACTION_HOVER_EXIT because every mouse-down on Android
            // follows a hover-exit and is followed by a hover-enter.  crbug.com/715114 filed on
            // distinguishing actual hover enter/exit from these bogus ones.
            if (eventAction == MotionEvent.ACTION_HOVER_ENTER
                    || eventAction == MotionEvent.ACTION_HOVER_EXIT) {
                return false;
            }

            // For mousedown and mouseup events, we use ACTION_BUTTON_PRESS
            // and ACTION_BUTTON_RELEASE respectively because they provide
            // info about the changed-button.
            if (eventAction == MotionEvent.ACTION_DOWN || eventAction == MotionEvent.ACTION_UP) {
                // While we use the action buttons for the changed state it is important to still
                // consume the down/up events to get the complete stream for a drag gesture, which
                // is provided using ACTION_MOVE touch events.
                return true;
            }

            nativeOnMouseEvent(mNativeEventForwarder, event.getEventTime(), eventAction,
                    offsetEvent.getX(), offsetEvent.getY(), event.getPointerId(0),
                    event.getPressure(0), event.getOrientation(0),
                    event.getAxisValue(MotionEvent.AXIS_TILT, 0), getMouseEventActionButton(event),
                    event.getButtonState(), event.getMetaState(), event.getToolType(0));
            return true;
        } finally {
            offsetEvent.recycle();
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    private int getMouseEventActionButton(MotionEvent event) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return event.getActionButton();

        // On <M, the only mice events sent are hover events, which cannot have a button.
        return 0;
    }

    public boolean onMouseWheelEvent(
            long timeMs, float x, float y, float ticksX, float ticksY, float pixelsPerTick) {
        assert mNativeEventForwarder != 0;
        nativeOnMouseWheelEvent(mNativeEventForwarder, timeMs, x, y, ticksX, ticksY, pixelsPerTick);
        return true;
    }

    /**
     * @see View#onDragEvent(DragEvent)
     * @param event {@link DragEvent} instance.
     * @param containerView A view on which the drag event is taking place.
     */
    @TargetApi(Build.VERSION_CODES.N)
    public boolean onDragEvent(DragEvent event, View containerView) {
        if (mNativeEventForwarder == 0 || Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            return false;
        }

        ClipDescription clipDescription = event.getClipDescription();

        // text/* will match text/uri-list, text/html, text/plain.
        String[] mimeTypes =
                clipDescription == null ? new String[0] : clipDescription.filterMimeTypes("text/*");

        if (event.getAction() == DragEvent.ACTION_DRAG_STARTED) {
            // TODO(hush): support dragging more than just text.
            return mimeTypes != null && mimeTypes.length > 0 && mIsDragDropEnabled;
        }

        StringBuilder content = new StringBuilder("");
        if (event.getAction() == DragEvent.ACTION_DROP) {
            // TODO(hush): obtain dragdrop permissions, when dragging files into Chrome/WebView is
            // supported. Not necessary to do so for now, because only text dragging is supported.
            ClipData clipData = event.getClipData();
            final int itemCount = clipData.getItemCount();
            for (int i = 0; i < itemCount; i++) {
                ClipData.Item item = clipData.getItemAt(i);
                content.append(item.coerceToStyledText(containerView.getContext()));
            }
        }

        int[] locationOnScreen = new int[2];
        containerView.getLocationOnScreen(locationOnScreen);

        // All coordinates are in device pixel. Conversion to DIP happens in the native.
        int x = (int) (event.getX() + mCurrentTouchOffsetX);
        int y = (int) (event.getY() + mCurrentTouchOffsetY);
        int screenX = x + locationOnScreen[0];
        int screenY = y + locationOnScreen[1];

        nativeOnDragEvent(mNativeEventForwarder, event.getAction(), x, y, screenX, screenY,
                mimeTypes, content.toString());
        return true;
    }

    /**
     * Forwards a gesture event.
     *
     * @param type Type of the gesture event.
     * @param timeMs Time the event occurred in milliseconds.
     * @param delta Scale factor for pinch gesture relative to the current state,
     *        1.0 being 100%. If negative, has the effect of reverting
     *        pinch scale to default.
     */
    public boolean onGestureEvent(@GestureEventType int type, long timeMs, float delta) {
        return nativeOnGestureEvent(mNativeEventForwarder, type, timeMs, delta);
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
    private native void nativeOnMouseWheelEvent(long nativeEventForwarder, long timeMs, float x,
            float y, float ticksX, float ticksY, float pixelsPerTick);
    private native void nativeOnDragEvent(long nativeEventForwarder, int action, int x, int y,
            int screenX, int screenY, String[] mimeTypes, String content);
    private native boolean nativeOnGestureEvent(
            long nativeEventForwarder, int type, long timeMs, float delta);
}
