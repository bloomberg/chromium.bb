// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.PointF;
import android.util.Log;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import java.util.HashMap;

/**
 * This class detects multi-finger tap events. This is provided since the stock Android
 * gesture-detectors only detect taps made with one finger.
 */
public class TapGestureDetector {
    /** The listener for receiving notifications of tap gestures. */
    public interface OnTapListener {
        /**
         * Notified when a tap event occurs.
         *
         * @param pointerCount The number of fingers that were tapped.
         * @return True if the event is consumed.
         */
        boolean onTap(int pointerCount);
    }

    /** The listener to which notifications are sent. */
    private OnTapListener mListener;

    /** The maximum number of fingers seen in the gesture. */
    private int mPointerCount = 0;

    /**
     * Stores the location of each down MotionEvent (by pointer ID), for detecting motion of any
     * pointer beyond the TouchSlop region.
     */
    private HashMap<Integer, PointF> mInitialPositions = new HashMap<Integer, PointF>();

    /**
     * Threshold squared-distance, in pixels, to use for motion-detection. If a finger moves less
     * than this distance, the gesture is still eligible to be a tap event.
     */
    private int mTouchSlopSquare;

    /** Set to true whenever motion is detected in the gesture. */
    private boolean mMotionOccurred = false;

    public TapGestureDetector(Context context, OnTapListener listener) {
        mListener = listener;
        ViewConfiguration config = ViewConfiguration.get(context);
        int touchSlop = config.getScaledTouchSlop();
        mTouchSlopSquare = touchSlop * touchSlop;
    }

    /** Analyzes the touch event to determine whether to notify the listener. */
    public boolean onTouchEvent(MotionEvent event) {
        boolean handled = false;
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                trackDownEvent(event);
                mPointerCount = Math.max(mPointerCount, event.getPointerCount());
                break;

            case MotionEvent.ACTION_MOVE:
                if (!mMotionOccurred) {
                    mMotionOccurred = trackMoveEvent(event);
                }
                break;

            case MotionEvent.ACTION_UP:
                if (!mMotionOccurred) {
                    handled = mListener.onTap(mPointerCount);
                }
                reset();
                break;

            case MotionEvent.ACTION_POINTER_UP:
                trackUpEvent(event);
                break;

            case MotionEvent.ACTION_CANCEL:
                reset();
                break;

            default:
                break;
        }
        return handled;
    }

    /** Stores the location of the ACTION_DOWN or ACTION_POINTER_DOWN event. */
    private void trackDownEvent(MotionEvent event) {
        int pointerIndex = 0;
        if (event.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN) {
            pointerIndex = event.getActionIndex();
        }
        int pointerId = event.getPointerId(pointerIndex);
        mInitialPositions.put(pointerId,
                new PointF(event.getX(pointerIndex), event.getY(pointerIndex)));
    }

    /** Removes the ACTION_UP or ACTION_POINTER_UP event from the stored list. */
    private void trackUpEvent(MotionEvent event) {
        int pointerIndex = 0;
        if (event.getActionMasked() == MotionEvent.ACTION_POINTER_UP) {
            pointerIndex = event.getActionIndex();
        }
        int pointerId = event.getPointerId(pointerIndex);
        mInitialPositions.remove(pointerId);
    }

    /**
     * Processes an ACTION_MOVE event and returns whether a pointer moved beyond the TouchSlop
     * threshold.
     *
     * @return True if motion was detected.
     */
    private boolean trackMoveEvent(MotionEvent event) {
        int pointerCount = event.getPointerCount();
        for (int i = 0; i < pointerCount; i++) {
            int pointerId = event.getPointerId(i);
            float currentX = event.getX(i);
            float currentY = event.getY(i);
            PointF downPoint = mInitialPositions.get(pointerId);
            if (downPoint == null) {
                // There was no corresponding DOWN event, so add it. This is an inconsistency
                // which shouldn't normally occur.
                mInitialPositions.put(pointerId, new PointF(currentX, currentY));
                continue;
            }
            float deltaX = currentX - downPoint.x;
            float deltaY = currentY - downPoint.y;
            if (deltaX * deltaX + deltaY * deltaY > mTouchSlopSquare) {
                return true;
            }
        }
        return false;
    }

    /** Cleans up any stored data for the gesture. */
    private void reset() {
        mPointerCount = 0;
        mInitialPositions.clear();
        mMotionOccurred = false;
    }
}
