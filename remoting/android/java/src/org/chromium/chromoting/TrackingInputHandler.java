// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.PointF;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

/**
 * This class implements the cursor-tracking behavior and gestures.
 */
public class TrackingInputHandler implements TouchInputHandler {
    /**
     * Minimum change to the scaling factor to be recognized as a zoom gesture. Setting lower
     * values here will result in more frequent canvas redraws during zooming.
     */
    private static final double MIN_ZOOM_DELTA = 0.05;

    private DesktopViewInterface mViewer;
    private RenderData mRenderData;

    private GestureDetector mScroller;
    private ScaleGestureDetector mZoomer;

    /**
     * The current cursor position is stored here as floats, so that the desktop image can be
     * positioned with sub-pixel accuracy, to give a smoother panning animation at high zoom levels.
     */
    private PointF mCursorPosition;

    private int mMouseButton;

    /**
     * Distinguish between finger tap and swipe. One-finger down then up should inject a
     * left-click event. But if the finger is dragged before being released, this should move
     * the cursor without injecting any button event. This flag is set when a motion event is
     * detected.
     */
    private boolean mFingerMoved = false;

    public TrackingInputHandler(DesktopViewInterface viewer, Context context,
                                RenderData renderData) {
        mViewer = viewer;
        mRenderData = renderData;

        GestureListener listener = new GestureListener();
        mScroller = new GestureDetector(context, listener, null, false);

        // If long-press is enabled, the gesture-detector will not emit any further onScroll
        // notifications after the onLongPress notification. Since onScroll is being used for
        // moving the cursor, it means that the cursor would become stuck if the finger were held
        // down too long.
        mScroller.setIsLongpressEnabled(false);

        mZoomer = new ScaleGestureDetector(context, listener);

        mCursorPosition = new PointF();

        mMouseButton = BUTTON_UNDEFINED;
        mFingerMoved = false;
    }

    /**
     * Moves the mouse-cursor, injects a mouse-move event and repositions the image.
     */
    private void moveCursor(float newX, float newY) {
        synchronized (mRenderData) {
            // Constrain cursor to the image area.
            if (newX < 0) newX = 0;
            if (newY < 0) newY = 0;
            if (newX > mRenderData.imageWidth) newX = mRenderData.imageWidth;
            if (newY > mRenderData.imageHeight) newY = mRenderData.imageHeight;
            mCursorPosition.set(newX, newY);
            repositionImage();
        }

        mViewer.injectMouseEvent((int)newX, (int)newY, BUTTON_UNDEFINED, false);
    }

    /**
     * Repositions the image by translating it (without affecting the zoom level) to place the
     * cursor close to the center of the screen.
     */
    private void repositionImage() {
        synchronized (mRenderData) {
            // Get the current cursor position in screen coordinates.
            float[] cursorScreen = {mCursorPosition.x, mCursorPosition.y};
            mRenderData.transform.mapPoints(cursorScreen);

            // Translate so the cursor is displayed in the middle of the screen.
            mRenderData.transform.postTranslate(
                    (float)mRenderData.screenWidth / 2 - cursorScreen[0],
                    (float)mRenderData.screenHeight / 2 - cursorScreen[1]);
        }
        mViewer.transformationChanged();
    }

    /** Injects a button event using the current cursor location. */
    private void injectButtonEvent(int button, boolean pressed) {
        mViewer.injectMouseEvent((int)mCursorPosition.x, (int)mCursorPosition.y, button, pressed);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // Avoid short-circuit logic evaluation - ensure both gesture detectors see all events so
        // that they generate correct notifications.
        boolean handled = mScroller.onTouchEvent(event);
        handled = mZoomer.onTouchEvent(event) || handled;

        int pointerCount = event.getPointerCount();

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                switch (pointerCount) {
                    case 1:
                        mFingerMoved = false;
                        mMouseButton = BUTTON_LEFT;
                        break;
                    case 2:
                        mMouseButton = BUTTON_RIGHT;
                        break;
                    case 3:
                        mMouseButton = BUTTON_UNDEFINED;
                        // TODO(lambroslambrou): Add 3-finger-tap for middle-click, and use 3-finger
                        // swipe to show the action-bar or keyboard.
                        break;
                    default:
                        break;
                }
                break;
            case MotionEvent.ACTION_UP:
                if (mFingerMoved) {
                    // Don't inject anything.
                    mFingerMoved = false;
                } else {
                    // The user pressed and released without moving. Inject a click event for a
                    // mouse button according to how many fingers were used.
                    injectButtonEvent(mMouseButton, true);
                    injectButtonEvent(mMouseButton, false);
                    handled = true;
                }
                break;
            case MotionEvent.ACTION_POINTER_UP:
                // |pointerCount| is the number of fingers that were on the screen prior to the UP
                // event.
                if (pointerCount == 3) {
                    mViewer.showActionBar();
                    handled = true;
                }
                break;
            default:
                break;
        }
        return handled;
    }

    @Override
    public void onScreenConfigurationChanged() {
    }

    @Override
    public void onClientSizeChanged(int width, int height) {
        repositionImage();
    }

    @Override
    public void onHostSizeChanged(int width, int height) {
        moveCursor((float)width / 2, (float)height / 2);
    }

    /** Responds to touch events filtered by the gesture detectors. */
    private class GestureListener extends GestureDetector.SimpleOnGestureListener
            implements ScaleGestureDetector.OnScaleGestureListener {
        /**
         * Called when the user drags one or more fingers across the touchscreen.
         */
        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            if (e2.getPointerCount() != 1) {
                return false;
            }
            mFingerMoved = true;

            float[] delta = {distanceX, distanceY};
            synchronized (mRenderData) {
                Matrix canvasToImage = new Matrix();
                mRenderData.transform.invert(canvasToImage);
                canvasToImage.mapVectors(delta);
            }

            moveCursor(mCursorPosition.x - delta[0], mCursorPosition.y - delta[1]);
            return true;
        }

        /** Called when the user is in the process of pinch-zooming. */
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            if (Math.abs(detector.getScaleFactor() - 1) < MIN_ZOOM_DELTA) {
                return false;
            }
            mFingerMoved = true;

            float scaleFactor = detector.getScaleFactor();
            synchronized (mRenderData) {
                mRenderData.transform.postScale(
                        scaleFactor, scaleFactor, detector.getFocusX(), detector.getFocusY());
            }
            repositionImage();
            return true;
        }

        /** Called whenever a gesture starts. Always accepts the gesture so it isn't ignored. */
        @Override
        public boolean onDown(MotionEvent e) {
            return true;
        }

        /**
         * Called when the user starts to zoom. Always accepts the zoom so that
         * onScale() can decide whether to respond to it.
         */
        @Override
        public boolean onScaleBegin(ScaleGestureDetector detector) {
            return true;
        }

        /** Called when the user is done zooming. Defers to onScale()'s judgement. */
        @Override
        public void onScaleEnd(ScaleGestureDetector detector) {
            onScale(detector);
        }
    }
}
