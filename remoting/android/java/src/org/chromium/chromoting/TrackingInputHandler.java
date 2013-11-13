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

    /**
     * Maximum allowed zoom level - see {@link #repositionImageWithZoom()}.
     */
    private static final float MAX_ZOOM_FACTOR = 100.0f;

    private DesktopViewInterface mViewer;
    private RenderData mRenderData;

    private GestureDetector mScroller;
    private ScaleGestureDetector mZoomer;
    private TapGestureDetector mTapDetector;

    /**
     * The current cursor position is stored here as floats, so that the desktop image can be
     * positioned with sub-pixel accuracy, to give a smoother panning animation at high zoom levels.
     */
    private PointF mCursorPosition;

    /**
     * Used for tracking swipe gestures. Only the Y-direction is needed for responding to swipe-up
     * or swipe-down.
     */
    private float mTotalMotionY = 0;

    /**
     * Set to true during the middle of a swipe gesture to prevent the remainder of the swipe
     * motion triggering further actions.
     */
    private boolean mSwipeCompleted = false;

    /**
     * Distance in pixels beyond which a motion gesture is considered to be a swipe. This is
     * initialized using the Context passed into the ctor.
     */
    private float mSwipeThreshold;

    /** Mouse-button currently held down, or BUTTON_UNDEFINED otherwise. */
    private int mHeldButton = BUTTON_UNDEFINED;

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
        mTapDetector = new TapGestureDetector(context, listener);

        mCursorPosition = new PointF();

        // The threshold needs to be bigger than the ScaledTouchSlop used by the gesture-detectors,
        // so that a gesture cannot be both a tap and a swipe. It also needs to be small enough so
        // that intentional swipes are usually detected.
        float density = context.getResources().getDisplayMetrics().density;
        mSwipeThreshold = 40 * density;
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

            // Now the cursor is displayed in the middle of the screen, see if the image can be
            // panned so that more of it is visible. The primary goal is to show as much of the
            // image as possible. The secondary goal is to keep the cursor in the middle.

            // Get the coordinates of the desktop rectangle (top-left/bottom-right corners) in
            // screen coordinates. Order is: left, top, right, bottom.
            float[] rectScreen = {0, 0, mRenderData.imageWidth, mRenderData.imageHeight};
            mRenderData.transform.mapPoints(rectScreen);

            float leftDelta = rectScreen[0];
            float rightDelta = rectScreen[2] - mRenderData.screenWidth;
            float topDelta = rectScreen[1];
            float bottomDelta = rectScreen[3] - mRenderData.screenHeight;
            float xAdjust = 0;
            float yAdjust = 0;

            if (rectScreen[2] - rectScreen[0] < mRenderData.screenWidth) {
                // Image is narrower than the screen, so center it.
                xAdjust = -(rightDelta + leftDelta) / 2;
            } else if (leftDelta > 0 && rightDelta > 0) {
                // Panning the image left will show more of it.
                xAdjust = -Math.min(leftDelta, rightDelta);
            } else if (leftDelta < 0 && rightDelta < 0) {
                // Pan the image right.
                xAdjust = Math.min(-leftDelta, -rightDelta);
            }

            // Apply similar logic for yAdjust.
            if (rectScreen[3] - rectScreen[1] < mRenderData.screenHeight) {
                yAdjust = -(bottomDelta + topDelta) / 2;
            } else if (topDelta > 0 && bottomDelta > 0) {
                yAdjust = -Math.min(topDelta, bottomDelta);
            } else if (topDelta < 0 && bottomDelta < 0) {
                yAdjust = Math.min(-topDelta, -bottomDelta);
            }

            mRenderData.transform.postTranslate(xAdjust, yAdjust);
        }
        mViewer.transformationChanged();
    }

    /**
     * Repositions the image by translating and zooming it, to keep the zoom level within sensible
     * limits. The minimum zoom level is chosen to avoid black space around all 4 sides. The
     * maximum zoom level is set arbitrarily, so that the user can zoom out again in a reasonable
     * time, and to prevent arithmetic overflow problems from displaying the image.
     */
    private void repositionImageWithZoom() {
        synchronized (mRenderData) {
            // Avoid division by zero in case this gets called before the image size is initialized.
            if (mRenderData.imageWidth == 0 || mRenderData.imageHeight == 0) {
                return;
            }

            // Zoom out if the zoom level is too high.
            float currentZoomLevel = mRenderData.transform.mapRadius(1.0f);
            if (currentZoomLevel > MAX_ZOOM_FACTOR) {
                mRenderData.transform.setScale(MAX_ZOOM_FACTOR, MAX_ZOOM_FACTOR);
            }

            // Get image size scaled to screen coordinates.
            float[] imageSize = {(float)mRenderData.imageWidth, (float)mRenderData.imageHeight};
            mRenderData.transform.mapVectors(imageSize);

            if (imageSize[0] < mRenderData.screenWidth && imageSize[1] < mRenderData.screenHeight) {
                // Displayed image is too small in both directions, so apply the minimum zoom
                // level needed to fit either the width or height.
                float scale = Math.min((float)mRenderData.screenWidth / mRenderData.imageWidth,
                                       (float)mRenderData.screenHeight / mRenderData.imageHeight);
                mRenderData.transform.setScale(scale, scale);
            }

            repositionImage();
        }
    }

    /** Injects a button event using the current cursor location. */
    private void injectButtonEvent(int button, boolean pressed) {
        mViewer.injectMouseEvent((int)mCursorPosition.x, (int)mCursorPosition.y, button, pressed);
    }

    /** Processes a (multi-finger) swipe gesture. */
    private boolean onSwipe() {
        if (mTotalMotionY > mSwipeThreshold) {
            // Swipe down occurred.
            mViewer.showActionBar();
            return true;
        } else if (mTotalMotionY < -mSwipeThreshold) {
            // Swipe up occurred.
            mViewer.showKeyboard();
            return true;
        }
        return false;
    }


    /** Injects a button-up event if the button is currently held down (during a drag event). */
    private void releaseAnyHeldButton() {
        if (mHeldButton != BUTTON_UNDEFINED) {
            injectButtonEvent(mHeldButton, false);
            mHeldButton = BUTTON_UNDEFINED;
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // Avoid short-circuit logic evaluation - ensure all gesture detectors see all events so
        // that they generate correct notifications.
        boolean handled = mScroller.onTouchEvent(event);
        handled |= mZoomer.onTouchEvent(event);
        handled |= mTapDetector.onTouchEvent(event);

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_POINTER_DOWN:
                mTotalMotionY = 0;
                mSwipeCompleted = false;
                break;

            case MotionEvent.ACTION_UP:
                releaseAnyHeldButton();
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
        repositionImageWithZoom();
    }

    @Override
    public void onHostSizeChanged(int width, int height) {
        moveCursor((float)width / 2, (float)height / 2);
        repositionImageWithZoom();
    }

    /** Responds to touch events filtered by the gesture detectors. */
    private class GestureListener extends GestureDetector.SimpleOnGestureListener
            implements ScaleGestureDetector.OnScaleGestureListener,
                       TapGestureDetector.OnTapListener {
        /**
         * Called when the user drags one or more fingers across the touchscreen.
         */
        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            if (e2.getPointerCount() == 3) {
                // Note that distance values are reversed. For example, dragging a finger in the
                // direction of increasing Y coordinate (downwards) results in distanceY being
                // negative.
                mTotalMotionY -= distanceY;
                if (!mSwipeCompleted) {
                    mSwipeCompleted = onSwipe();
                    return mSwipeCompleted;
                }
            }

            if (e2.getPointerCount() != 1) {
                return false;
            }

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

            float scaleFactor = detector.getScaleFactor();
            synchronized (mRenderData) {
                mRenderData.transform.postScale(
                        scaleFactor, scaleFactor, detector.getFocusX(), detector.getFocusY());
            }
            repositionImageWithZoom();
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

        /** Maps the number of fingers in a tap or long-press gesture to a mouse-button. */
        private int mouseButtonFromPointerCount(int pointerCount) {
            switch (pointerCount) {
                case 1:
                    return BUTTON_LEFT;
                case 2:
                    return BUTTON_RIGHT;
                case 3:
                    return BUTTON_MIDDLE;
                default:
                    return BUTTON_UNDEFINED;
            }
        }

        /** Called when the user taps the screen with one or more fingers. */
        @Override
        public boolean onTap(int pointerCount) {
            int button = mouseButtonFromPointerCount(pointerCount);
            if (button == BUTTON_UNDEFINED) {
                return false;
            } else {
                injectButtonEvent(button, true);
                injectButtonEvent(button, false);
                return true;
            }
        }

        /** Called when a long-press is triggered for one or more fingers. */
        @Override
        public void onLongPress(int pointerCount) {
            mHeldButton = mouseButtonFromPointerCount(pointerCount);
            if (mHeldButton != BUTTON_UNDEFINED) {
                injectButtonEvent(mHeldButton, true);
                mViewer.showLongPressFeedback();
            }
        }
    }
}
