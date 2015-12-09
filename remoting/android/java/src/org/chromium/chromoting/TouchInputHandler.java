// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.PointF;
import android.graphics.Rect;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.widget.Scroller;

/**
 * This class is responsible for handling Touch input from the user.  Touch events which manipulate
 * the local canvas are handled in this class and any input which should be sent to the remote host
 * are passed to the InputStrategyInterface implementation set by the DesktopView.
 */
public class TouchInputHandler implements TouchInputHandlerInterface {
    /**
     * Minimum change to the scaling factor to be recognized as a zoom gesture. Setting lower
     * values here will result in more frequent canvas redraws during zooming.
     */
    private static final double MIN_ZOOM_DELTA = 0.05;

    private final DesktopViewInterface mViewer;
    private final RenderData mRenderData;
    private final DesktopCanvas mDesktopCanvas;
    private InputStrategyInterface mInputStrategy;

    private GestureDetector mScroller;
    private ScaleGestureDetector mZoomer;
    private TapGestureDetector mTapDetector;

    /** Used to calculate the physics for flinging the viewport across the desktop image. */
    private Scroller mFlingScroller;

    /** Used to disambiguate a 2-finger gesture as a swipe or a pinch. */
    private SwipePinchDetector mSwipePinchDetector;

    /**
     * Used for tracking swipe gestures. Only the Y-direction is needed for responding to swipe-up
     * or swipe-down.
     */
    private float mTotalMotionY = 0;

    /**
     * Distance in pixels beyond which a motion gesture is considered to be a swipe. This is
     * initialized using the Context passed into the ctor.
     */
    private float mSwipeThreshold;

    /**
     * Set to true to prevent any further movement of the cursor, for example, when showing the
     * keyboard to prevent the cursor wandering from the area where keystrokes should be sent.
     */
    private boolean mSuppressCursorMovement = false;

    /**
     * Set to true to suppress the fling animation at the end of a gesture, for example, when
     * dragging whilst a button is held down.
     */
    private boolean mSuppressFling = false;

    /**
     * Set to true when 3-finger swipe gesture is complete, so that further movement doesn't
     * trigger more swipe actions.
     */
    private boolean mSwipeCompleted = false;

    /**
     * Set to true when a 1 finger pan gesture originates with a longpress.  This means the user
     * is performing a drag operation.
     */
    private boolean mIsDragging = false;

    public TouchInputHandler(DesktopViewInterface viewer, Context context, RenderData renderData) {
        mViewer = viewer;
        mRenderData = renderData;
        mDesktopCanvas = new DesktopCanvas(mViewer, mRenderData);

        GestureListener listener = new GestureListener();
        mScroller = new GestureDetector(context, listener, null, false);

        // If long-press is enabled, the gesture-detector will not emit any further onScroll
        // notifications after the onLongPress notification. Since onScroll is being used for
        // moving the cursor, it means that the cursor would become stuck if the finger were held
        // down too long.
        mScroller.setIsLongpressEnabled(false);

        mZoomer = new ScaleGestureDetector(context, listener);
        mTapDetector = new TapGestureDetector(context, listener);
        mFlingScroller = new Scroller(context);
        mSwipePinchDetector = new SwipePinchDetector(context);

        // The threshold needs to be bigger than the ScaledTouchSlop used by the gesture-detectors,
        // so that a gesture cannot be both a tap and a swipe. It also needs to be small enough so
        // that intentional swipes are usually detected.
        float density = context.getResources().getDisplayMetrics().density;
        mSwipeThreshold = 40 * density;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // Give the underlying input strategy a chance to observe the current motion event before
        // passing it to the gesture detectors.  This allows the input strategy to react to the
        // event or save the payload for use in recreating the gesture remotely.
        mInputStrategy.onMotionEvent(event);

        // Avoid short-circuit logic evaluation - ensure all gesture detectors see all events so
        // that they generate correct notifications.
        boolean handled = mScroller.onTouchEvent(event);
        handled |= mZoomer.onTouchEvent(event);
        handled |= mTapDetector.onTouchEvent(event);
        mSwipePinchDetector.onTouchEvent(event);

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                mViewer.setAnimationEnabled(false);
                mSuppressCursorMovement = false;
                mSuppressFling = false;
                mSwipeCompleted = false;
                mIsDragging = false;
                break;

            case MotionEvent.ACTION_POINTER_DOWN:
                mTotalMotionY = 0;
                break;

            default:
                break;
        }
        return handled;
    }

    @Override
    public void onClientSizeChanged(int width, int height) {
        mDesktopCanvas.repositionImageWithZoom();
    }

    @Override
    public void onHostSizeChanged(int width, int height) {
        moveViewport((float) width / 2, (float) height / 2);
        mDesktopCanvas.resizeImageToFitScreen();
    }

    @Override
    public void onSoftInputMethodVisibilityChanged(boolean inputMethodVisible, Rect bounds) {
        synchronized (mRenderData) {
            if (inputMethodVisible) {
                mDesktopCanvas.setInputMethodOffsetValues(mRenderData.screenWidth - bounds.right,
                                                          mRenderData.screenHeight - bounds.bottom);
            } else {
                mDesktopCanvas.setInputMethodOffsetValues(0, 0);
            }
        }

        mDesktopCanvas.repositionImageWithZoom();
    }

    @Override
    public void processAnimation() {
        int previousX = mFlingScroller.getCurrX();
        int previousY = mFlingScroller.getCurrY();
        if (!mFlingScroller.computeScrollOffset()) {
            mViewer.setAnimationEnabled(false);
            return;
        }
        int deltaX = mFlingScroller.getCurrX() - previousX;
        int deltaY = mFlingScroller.getCurrY() - previousY;
        float[] delta = {deltaX, deltaY};
        synchronized (mRenderData) {
            Matrix canvasToImage = new Matrix();
            mRenderData.transform.invert(canvasToImage);
            canvasToImage.mapVectors(delta);
        }

        moveViewportWithOffset(-delta[0], -delta[1]);
    }

    @Override
    public void setInputStrategy(InputStrategyInterface inputStrategy) {
        mInputStrategy = inputStrategy;
    }

    /** Moves the desired center of the viewport using the specified deltas. */
    private void moveViewportWithOffset(float deltaX, float deltaY) {
        // If we are in an indirect mode or are in the middle of a drag operation, then we want to
        // invert the direction of the operation (i.e. follow the motion of the finger).
        if (mInputStrategy.isIndirectInputMode() || mIsDragging) {
            deltaX = -deltaX;
            deltaY = -deltaY;
        }

        // Constrain the coordinates to the image area.
        PointF viewportPosition = mDesktopCanvas.getViewportPosition();
        float newX = viewportPosition.x + deltaX;
        float newY = viewportPosition.y + deltaY;
        synchronized (mRenderData) {
            // Constrain viewport position to the image area.
            if (newX < 0) {
                newX = 0;
            } else if (newX > mRenderData.imageWidth) {
                newX = mRenderData.imageWidth;
            }

            if (newY < 0) {
                newY = 0;
            } else if (newY > mRenderData.imageHeight) {
                newY = mRenderData.imageHeight;
            }
        }

        moveViewport(newX, newY);
    }

    /** Moves the desired center of the viewport to the specified position. */
    private void moveViewport(float newX, float newY) {
        mDesktopCanvas.setViewportPosition(newX, newY);

        // If we are in an indirect mode or are in the middle of a drag operation, then we want to
        // keep the cursor centered, if possible, as the viewport moves.
        if (mInputStrategy.isIndirectInputMode() || mIsDragging) {
            moveCursor((int) newX, (int) newY);
        }

        mDesktopCanvas.repositionImage();
    }

    /** Moves the cursor to the specified position on the screen. */
    private void moveCursorToScreenPoint(float screenX, float screenY) {
        float[] mappedValues = {screenX, screenY};
        synchronized (mRenderData) {
            Matrix canvasToImage = new Matrix();
            mRenderData.transform.invert(canvasToImage);
            canvasToImage.mapPoints(mappedValues);
        }
        moveCursor((int) mappedValues[0], (int) mappedValues[1]);
    }

    /** Moves the cursor to the specified position on the remote host. */
    private void moveCursor(int newX, int newY) {
        synchronized (mRenderData) {
            boolean cursorMoved = mRenderData.setCursorPosition(newX, newY);
            if (cursorMoved) {
                mInputStrategy.injectCursorMoveEvent(newX, newY);
            }
        }
    }

    /** Processes a (multi-finger) swipe gesture. */
    private boolean onSwipe() {
        if (mTotalMotionY > mSwipeThreshold) {
            // Swipe down occurred.
            mViewer.showActionBar();
        } else if (mTotalMotionY < -mSwipeThreshold) {
            // Swipe up occurred.
            mViewer.showKeyboard();
        } else {
            return false;
        }

        mSuppressCursorMovement = true;
        mSuppressFling = true;
        mSwipeCompleted = true;
        return true;
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
            int pointerCount = e2.getPointerCount();
            if (pointerCount == 3 && !mSwipeCompleted) {
                // Note that distance values are reversed. For example, dragging a finger in the
                // direction of increasing Y coordinate (downwards) results in distanceY being
                // negative.
                mTotalMotionY -= distanceY;
                return onSwipe();
            }

            if (pointerCount == 2 && mSwipePinchDetector.isSwiping()) {
                mInputStrategy.onScroll(distanceX, distanceY);

                // Prevent the cursor being moved or flung by the gesture.
                mSuppressCursorMovement = true;
                return true;
            }

            if (pointerCount != 1 || mSuppressCursorMovement) {
                return false;
            }

            float[] delta = {distanceX, distanceY};
            synchronized (mRenderData) {
                Matrix canvasToImage = new Matrix();
                mRenderData.transform.invert(canvasToImage);
                canvasToImage.mapVectors(delta);
            }

            moveViewportWithOffset(delta[0], delta[1]);
            return true;
        }

        /**
         * Called when a fling gesture is recognized.
         */
        @Override
        public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
            // If cursor movement is suppressed, fling also needs to be suppressed, as the
            // gesture-detector will still generate onFling() notifications based on movement of
            // the fingers, which would result in unwanted cursor movement.
            if (mSuppressCursorMovement || mSuppressFling) {
                return false;
            }

            // The fling physics calculation is based on screen coordinates, so that it will
            // behave consistently at different zoom levels (and will work nicely at high zoom
            // levels, since |mFlingScroller| outputs integer coordinates). However, the desktop
            // will usually be panned as the cursor is moved across the desktop, which means the
            // transformation mapping from screen to desktop coordinates will change. To deal with
            // this, the cursor movement is computed from relative coordinate changes from
            // |mFlingScroller|. This means the fling can be started at (0, 0) with no bounding
            // constraints - the cursor is already constrained by the desktop size.
            mFlingScroller.fling(0, 0, (int) velocityX, (int) velocityY, Integer.MIN_VALUE,
                    Integer.MAX_VALUE, Integer.MIN_VALUE, Integer.MAX_VALUE);
            // Initialize the scroller's current offset coordinates, since they are used for
            // calculating the delta values.
            mFlingScroller.computeScrollOffset();
            mViewer.setAnimationEnabled(true);
            return true;
        }

        /** Called when the user is in the process of pinch-zooming. */
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            if (!mSwipePinchDetector.isPinching()) {
                return false;
            }

            if (Math.abs(detector.getScaleFactor() - 1) < MIN_ZOOM_DELTA) {
                return false;
            }

            float scaleFactor = detector.getScaleFactor();
            synchronized (mRenderData) {
                mRenderData.transform.postScale(
                        scaleFactor, scaleFactor, detector.getFocusX(), detector.getFocusY());
            }
            mDesktopCanvas.repositionImageWithZoom();

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

        /** Called when the user taps the screen with one or more fingers. */
        @Override
        public boolean onTap(int pointerCount, float x, float y) {
            int button = mouseButtonFromPointerCount(pointerCount);
            if (button == BUTTON_UNDEFINED) {
                return false;
            }

            if (!mInputStrategy.isIndirectInputMode()) {
                moveCursorToScreenPoint(x, y);
            }

            if (mInputStrategy.onTap(button)) {
                mViewer.showInputFeedback(mInputStrategy.getShortPressFeedbackType());
            }
            return true;
        }

        /** Called when a long-press is triggered for one or more fingers. */
        @Override
        public void onLongPress(int pointerCount, float x, float y) {
            int button = mouseButtonFromPointerCount(pointerCount);
            if (button == BUTTON_UNDEFINED) {
                return;
            }

            if (!mInputStrategy.isIndirectInputMode()) {
                moveCursorToScreenPoint(x, y);
            }

            if (mInputStrategy.onPressAndHold(button)) {
                mViewer.showInputFeedback(mInputStrategy.getLongPressFeedbackType());
                mSuppressFling = true;
                mIsDragging = true;
            }
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
    }
}
