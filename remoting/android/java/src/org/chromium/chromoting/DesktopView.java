// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.os.Bundle;
import android.os.Looper;
import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

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
    private static int MIN_SCROLL_DISTANCE = 8 * 8;

    /**
     * Minimum change to the scaling factor to be recognized as a zoom gesture. Setting lower
     * values here will result in more frequent canvas redraws during zooming.
     */
    private static double MIN_ZOOM_FACTOR = 0.05;

    /** Specifies one dimension of an image. */
    private static enum Constraint {
        UNDEFINED, WIDTH, HEIGHT
    }

    private GestureDetector mScroller;
    private ScaleGestureDetector mZoomer;

    /** Stores pan and zoom configuration and converts image coordinates to screen coordinates. */
    private Matrix mTransform;

    private int mScreenWidth;
    private int mScreenHeight;

    /** Specifies the dimension by which the zoom level is being lower-bounded. */
    private Constraint mConstraint;

    /** Whether the right edge of the image was visible on-screen during the last render. */
    private boolean mRightUsedToBeOut;

    /** Whether the bottom edge of the image was visible on-screen during the last render. */
    private boolean mBottomUsedToBeOut;

    /** Whether the device has just been rotated, necessitating a canvas redraw. */
    private boolean mJustRotated;

    public DesktopView(Context context) {
        super(context);
        getHolder().addCallback(this);
        DesktopListener listener = new DesktopListener();
        mScroller = new GestureDetector(context, listener);
        mZoomer = new ScaleGestureDetector(context, listener);

        mTransform = new Matrix();
        mScreenWidth = 0;
        mScreenHeight = 0;
        mConstraint = Constraint.UNDEFINED;

        mRightUsedToBeOut = false;
        mBottomUsedToBeOut = false;

        mJustRotated = false;
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
                mConstraint = image.getWidth()/image.getHeight() > mScreenWidth/mScreenHeight ?
                        Constraint.WIDTH : Constraint.HEIGHT;
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

                if (leftEdgeOutOfBounds != rightEdgeOutOfBounds) {
                    if (leftEdgeOutOfBounds != mRightUsedToBeOut) {
                        values[Matrix.MTRANS_X] = 0;
                    }
                    else {
                        values[Matrix.MTRANS_X] += mScreenWidth - bottomright[0];
                    }
                }
                else {  // The view would oscillate if this were updated while scrolling off-screen.
                    mRightUsedToBeOut = rightEdgeOutOfBounds;
                }

                if (topEdgeOutOfBounds != bottomEdgeOutOfBounds) {
                    if (topEdgeOutOfBounds != mBottomUsedToBeOut) {
                        values[Matrix.MTRANS_Y] = 0;
                    }
                    else {
                        values[Matrix.MTRANS_Y] += mScreenHeight - bottomright[1];
                    }
                }
                else {  // The view would oscillate if this were updated while scrolling off-screen.
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

    /** Causes the canvas to be redrawn the next time our surface changes. */
    public void announceScreenRotation() {
        mJustRotated = true;
    }

    /**
     * Called after the canvas is initially created, then after every
     * subsequent resize, as when the display is rotated.
     */
    @Override
    public void surfaceChanged(
            SurfaceHolder holder, int format, int width, int height) {
        synchronized (mTransform) {
            mScreenWidth = width;
            mScreenHeight = height;
            mConstraint = Constraint.UNDEFINED;
        }

        // If the size actually *changed*, we need to redraw the canvas.
        if (mJustRotated) {
            JniInterface.redrawGraphics();
            mJustRotated = false;
        }
    }

    /** Called when the canvas is first created. */
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i("deskview", "DesktopView.surfaceCreated(...)");
        JniInterface.provideRedrawCallback(this);
    }

    /** Called when the canvas is finally destroyed. */
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i("deskview", "DesktopView.surfaceDestroyed(...)");
    }

    /**
     * Called whenever the user attempts to touch the canvas. Forwards such
     * events to the appropriate gesture detector until one accepts them.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return mScroller.onTouchEvent(event) || mZoomer.onTouchEvent(event);
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

        /**
         * Called when the user starts to zoom. Always accepts the zoom so that
         * onScale() can decide whether to respond to it.
         */
        @Override
        public boolean onScaleBegin(ScaleGestureDetector detector) {
            return true;
        }

        /**
         * Called when the user is done zooming. Defers to onScale()'s judgement.
         */
        @Override
        public void onScaleEnd(ScaleGestureDetector detector) {
            onScale(detector);
        }
    }
}
