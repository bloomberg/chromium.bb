// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.ActionBar;
import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.os.Bundle;
import android.os.Looper;
import android.text.InputType;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import org.chromium.chromoting.jni.JniInterface;

/**
 * The user interface for viewing and interacting with a specific remote host.
 * It provides a canvas onto which the video feed is rendered, handles
 * multitouch pan and zoom gestures, and collects and forwards input events.
 */
/** GUI element that holds the drawing canvas. */
public class DesktopView extends SurfaceView implements Runnable, SurfaceHolder.Callback {
    /**
     * *Square* of the minimum displacement (in pixels) to be recognized as a scroll gesture.
     * Setting this to a lower value forces more frequent canvas redraws during scrolling.
     */
    private static final int MIN_SCROLL_DISTANCE = 8 * 8;

    /**
     * Minimum change to the scaling factor to be recognized as a zoom gesture. Setting lower
     * values here will result in more frequent canvas redraws during zooming.
     */
    private static final double MIN_ZOOM_FACTOR = 0.05;

    /*
     * These constants must match those in the generated struct protoc::MouseEvent_MouseButton.
     */
    private static final int BUTTON_UNDEFINED = 0;
    private static final int BUTTON_LEFT = 1;
    private static final int BUTTON_RIGHT = 3;

    /** Specifies one dimension of an image. */
    private static enum Constraint {
        UNDEFINED, WIDTH, HEIGHT
    }

    private ActionBar mActionBar;

    private GestureDetector mScroller;
    private ScaleGestureDetector mZoomer;

    /** Stores pan and zoom configuration and converts image coordinates to screen coordinates. */
    private Matrix mTransform;

    private int mScreenWidth;
    private int mScreenHeight;

    /** Specifies the dimension by which the zoom level is being lower-bounded. */
    private Constraint mConstraint;

    /** Whether the dimension of constraint should be reckecked on the next aspect ratio change. */
    private boolean mRecheckConstraint;

    /** Whether the right edge of the image was visible on-screen during the last render. */
    private boolean mRightUsedToBeOut;

    /** Whether the bottom edge of the image was visible on-screen during the last render. */
    private boolean mBottomUsedToBeOut;

    private int mMouseButton;
    private boolean mMousePressed;

    public DesktopView(Activity context) {
        super(context);

        // Give this view keyboard focus, allowing us to customize the soft keyboard's settings.
        setFocusableInTouchMode(true);

        mActionBar = context.getActionBar();

        getHolder().addCallback(this);
        DesktopListener listener = new DesktopListener();
        mScroller = new GestureDetector(context, listener, null, false);
        mZoomer = new ScaleGestureDetector(context, listener);

        mTransform = new Matrix();
        mScreenWidth = 0;
        mScreenHeight = 0;

        mConstraint = Constraint.UNDEFINED;
        mRecheckConstraint = false;

        mRightUsedToBeOut = false;
        mBottomUsedToBeOut = false;

        mMouseButton = BUTTON_UNDEFINED;
        mMousePressed = false;
    }

    /**
     * Redraws the canvas. This should be done on a non-UI thread or it could
     * cause the UI to lag. Specifically, it is currently invoked on the native
     * graphics thread using a JNI.
     */
    @Override
    public void run() {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w("deskview", "Canvas being redrawn on UI thread");
        }

        Bitmap image = JniInterface.retrieveVideoFrame();
        Canvas canvas = getHolder().lockCanvas();
        synchronized (mTransform) {
            canvas.setMatrix(mTransform);

            // Internal parameters of the transformation matrix.
            float[] values = new float[9];
            mTransform.getValues(values);

            // Screen coordinates of two defining points of the image.
            float[] topleft = {0, 0};
            mTransform.mapPoints(topleft);
            float[] bottomright = {image.getWidth(), image.getHeight()};
            mTransform.mapPoints(bottomright);

            // Whether to rescale and recenter the view.
            boolean recenter = false;

            if (mConstraint == Constraint.UNDEFINED) {
                mConstraint = (double)image.getWidth()/image.getHeight() >
                        (double)mScreenWidth/mScreenHeight ? Constraint.WIDTH : Constraint.HEIGHT;
                recenter = true;  // We always rescale and recenter after a rotation.
            }

            if (mConstraint == Constraint.WIDTH &&
                    ((int)(bottomright[0] - topleft[0] + 0.5) < mScreenWidth || recenter)) {
                // The vertical edges of the image are flush against the device's screen edges
                // when the entire host screen is visible, and the user has zoomed out too far.
                float imageMiddle = (float)image.getHeight() / 2;
                float screenMiddle = (float)mScreenHeight / 2;
                mTransform.setPolyToPoly(
                        new float[] {0, imageMiddle, image.getWidth(), imageMiddle}, 0,
                        new float[] {0, screenMiddle, mScreenWidth, screenMiddle}, 0, 2);
            } else if (mConstraint == Constraint.HEIGHT &&
                    ((int)(bottomright[1] - topleft[1] + 0.5) < mScreenHeight || recenter)) {
                // The horizontal image edges are flush against the device's screen edges when
                // the entire host screen is visible, and the user has zoomed out too far.
                float imageCenter = (float)image.getWidth() / 2;
                float screenCenter = (float)mScreenWidth / 2;
                mTransform.setPolyToPoly(
                        new float[] {imageCenter, 0, imageCenter, image.getHeight()}, 0,
                        new float[] {screenCenter, 0, screenCenter, mScreenHeight}, 0, 2);
            } else {
                // It's fine for both members of a pair of image edges to be within the screen
                // edges (or "out of bounds"); that simply means that the image is zoomed out as
                // far as permissible. And both members of a pair can obviously be outside the
                // screen's edges, which indicates that the image is zoomed in to far to see the
                // whole host screen. However, if only one of a pair of edges has entered the
                // screen, the user is attempting to scroll into a blank area of the canvas.

                // A value of true means the corresponding edge has entered the screen's borders.
                boolean leftEdgeOutOfBounds = values[Matrix.MTRANS_X] > 0;
                boolean topEdgeOutOfBounds = values[Matrix.MTRANS_Y] > 0;
                boolean rightEdgeOutOfBounds = bottomright[0] < mScreenWidth;
                boolean bottomEdgeOutOfBounds = bottomright[1] < mScreenHeight;

                // Prevent the user from scrolling past the left or right edge of the image.
                if (leftEdgeOutOfBounds != rightEdgeOutOfBounds) {
                    if (leftEdgeOutOfBounds != mRightUsedToBeOut) {
                        // Make the left edge of the image flush with the left screen edge.
                        values[Matrix.MTRANS_X] = 0;
                    }
                    else {
                        // Make the right edge of the image flush with the right screen edge.
                        values[Matrix.MTRANS_X] += mScreenWidth - bottomright[0];
                    }
                } else {
                    // The else prevents this from being updated during the repositioning process,
                    // in which case the view would begin to oscillate.
                    mRightUsedToBeOut = rightEdgeOutOfBounds;
                }

                // Prevent the user from scrolling past the top or bottom edge of the image.
                if (topEdgeOutOfBounds != bottomEdgeOutOfBounds) {
                    if (topEdgeOutOfBounds != mBottomUsedToBeOut) {
                        // Make the top edge of the image flush with the top screen edge.
                        values[Matrix.MTRANS_Y] = 0;
                    } else {
                        // Make the bottom edge of the image flush with the bottom screen edge.
                        values[Matrix.MTRANS_Y] += mScreenHeight - bottomright[1];
                    }
                }
                else {
                    // The else prevents this from being updated during the repositioning process,
                    // in which case the view would begin to oscillate.
                    mBottomUsedToBeOut = bottomEdgeOutOfBounds;
                }

                mTransform.setValues(values);
            }

            canvas.setMatrix(mTransform);
        }

        canvas.drawColor(Color.BLACK);
        canvas.drawBitmap(image, 0, 0, new Paint());
        getHolder().unlockCanvasAndPost(canvas);
    }

    /**
     * Causes the next canvas redraw to perform a check for which screen dimension more tightly
     * constrains the view of the image. This should be called between the time that a screen size
     * change is requested and the time it actually occurs. If it is not called in such a case, the
     * screen will not be rearranged as aggressively (which is desirable when the software keyboard
     * appears in order to allow it to cover the image without forcing a resize).
     */
    public void requestRecheckConstrainingDimension() {
        mRecheckConstraint = true;
    }

    /**
     * Called after the canvas is initially created, then after every
     * subsequent resize, as when the display is rotated.
     */
    @Override
    public void surfaceChanged(
            SurfaceHolder holder, int format, int width, int height) {
        mActionBar.hide();

        synchronized (mTransform) {
            mScreenWidth = width;
            mScreenHeight = height;

            if (mRecheckConstraint) {
                mConstraint = Constraint.UNDEFINED;
                mRecheckConstraint = false;
            }
        }

        if (!JniInterface.redrawGraphics()) {
            JniInterface.provideRedrawCallback(this);
        }
    }

    /** Called when the canvas is first created. */
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i("deskview", "DesktopView.surfaceCreated(...)");
    }

    /**
     * Called when the canvas is finally destroyed. Marks the canvas as needing a redraw so that it
     * will not be blank if the user later switches back to our window.
     */
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i("deskview", "DesktopView.surfaceDestroyed(...)");

        // Stop this canvas from being redrawn.
        JniInterface.provideRedrawCallback(null);
    }

    /** Called when a software keyboard is requested, and specifies its options. */
    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        // Disables rich input support and instead requests simple key events.
        outAttrs.inputType = InputType.TYPE_NULL;

        // Prevents most third-party IMEs from ignoring our Activity's adjustResize preference.
        outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_FULLSCREEN;

        // Ensures that keyboards will not decide to hide the remote desktop on small displays.
        outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_EXTRACT_UI;

        // Stops software keyboards from closing as soon as the enter key is pressed.
        outAttrs.imeOptions |= EditorInfo.IME_MASK_ACTION | EditorInfo.IME_FLAG_NO_ENTER_ACTION;

        return null;
    }

    /** Called when a mouse action is made. */
    private void handleMouseMovement(float x, float y, int button, boolean pressed) {
        float[] coordinates = {x, y};

        // Coordinates are relative to the canvas, but we need image coordinates.
        Matrix canvasToImage = new Matrix();
        mTransform.invert(canvasToImage);
        canvasToImage.mapPoints(coordinates);

        // Coordinates are now relative to the image, so transmit them to the host.
        JniInterface.mouseAction((int)coordinates[0], (int)coordinates[1], button, pressed);
    }

    /**
     * Called whenever the user attempts to touch the canvas. Forwards such
     * events to the appropriate gesture detector until one accepts them.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getPointerCount() == 3) {
            mActionBar.show();
        }

        boolean handled = mScroller.onTouchEvent(event) || mZoomer.onTouchEvent(event);

        if (event.getPointerCount() == 1) {
            float x = event.getRawX();
            float y = event.getY();

            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    Log.i("mouse", "Found a finger");
                    mMouseButton = BUTTON_UNDEFINED;
                    mMousePressed = false;
                    break;

                case MotionEvent.ACTION_MOVE:
                    Log.i("mouse", "Finger is dragging");
                    if (mMouseButton == BUTTON_UNDEFINED) {
                        Log.i("mouse", "\tStarting left click");
                        mMouseButton = BUTTON_LEFT;
                        mMousePressed = true;
                    }
                    break;

                case MotionEvent.ACTION_UP:
                    Log.i("mouse", "Lost the finger");
                    if (mMouseButton == BUTTON_UNDEFINED) {
                        // The user pressed and released without moving: do left click and release.
                        Log.i("mouse", "\tStarting and finishing left click");
                        handleMouseMovement(x, y, BUTTON_LEFT, true);
                        mMouseButton = BUTTON_LEFT;
                        mMousePressed = false;
                    }
                    else if (mMousePressed) {
                        Log.i("mouse", "\tReleasing the currently-pressed button");
                        mMousePressed = false;
                    }
                    else {
                        Log.w("mouse", "Button already in released state before gesture ended");
                    }
                    break;

                default:
                    return handled;
            }
            handleMouseMovement(x, y, mMouseButton, mMousePressed);

            return true;
        }

        return handled;
    }

    /** Responds to touch events filtered by the gesture detectors. */
    private class DesktopListener extends GestureDetector.SimpleOnGestureListener
            implements ScaleGestureDetector.OnScaleGestureListener {
        /**
         * Called when the user is scrolling. We refuse to accept or process the event unless it
         * is being performed with 2 or more touch points, in order to reserve single-point touch
         * events for emulating mouse input.
         */
        @Override
        public boolean onScroll(
                MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            if (e2.getPointerCount() < 2 ||
                    Math.pow(distanceX, 2) + Math.pow(distanceY, 2) < MIN_SCROLL_DISTANCE) {
                return false;
            }

            synchronized (mTransform) {
                mTransform.postTranslate(-distanceX, -distanceY);
            }
            JniInterface.redrawGraphics();
            return true;
        }

        /** Called when the user is in the process of pinch-zooming. */
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            if (Math.abs(detector.getScaleFactor() - 1) < MIN_ZOOM_FACTOR) {
                return false;
            }

            synchronized (mTransform) {
                float scaleFactor = detector.getScaleFactor();
                mTransform.postScale(
                        scaleFactor, scaleFactor, detector.getFocusX(), detector.getFocusY());
            }
            JniInterface.redrawGraphics();
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

        /** Called when the user holds down on the screen. Starts a right-click. */
        @Override
        public void onLongPress(MotionEvent e) {
            if (e.getPointerCount() > 1) {
                return;
            }

            float x = e.getRawX();
            float y = e.getY();

            Log.i("mouse", "Finger held down");
            if (mMousePressed) {
                Log.i("mouse", "\tReleasing the currently-pressed button");
                handleMouseMovement(x, y, mMouseButton, false);
            }

            Log.i("mouse", "\tStarting right click");
            mMouseButton = BUTTON_RIGHT;
            mMousePressed = true;
            handleMouseMovement(x, y, mMouseButton, mMousePressed);
        }
    }
}
