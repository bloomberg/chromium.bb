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
    }

    /**
     * Redraws the canvas. This should be done on a non-UI thread or it could
     * cause the UI to lag. Specifically, it is currently invoked on the native
     * graphics thread using a JNI.
     */
    @Override
    public void run() {
        if (Looper.myLooper()==Looper.getMainLooper()) {
            Log.w("deskview", "Canvas being redrawn on UI thread");
        }

        Canvas canvas = getHolder().lockCanvas();
        Bitmap image = JniInterface.retrieveVideoFrame();
        synchronized (mTransform) {
            canvas.setMatrix(mTransform);

            float[] values = new float[9];
            mTransform.getValues(values);
            float[] topleft = {0, 0};
            mTransform.mapPoints(topleft);
            float[] bottomright = {image.getWidth(), image.getHeight()};
            mTransform.mapPoints(bottomright);

            if (mConstraint==Constraint.UNDEFINED) {
                mConstraint = image.getWidth()/image.getHeight() > mScreenWidth/mScreenHeight ?
                        Constraint.HEIGHT : Constraint.WIDTH;
            }

            if (mConstraint==Constraint.WIDTH && bottomright[0] - topleft[0] < mScreenWidth) {
                mTransform.setPolyToPoly(new float[] {0, 0, image.getWidth(), 0}, 0,
                        new float[] {0, 0, mScreenWidth, 0}, 0, 2);
            } else if (mConstraint==Constraint.HEIGHT &&
                    bottomright[1] - topleft[1] < mScreenHeight) {
                mTransform.setPolyToPoly(new float[] {0, 0, 0, image.getHeight()}, 0,
                        new float[] {0, 0, 0, mScreenHeight}, 0, 2);
            } else {
                if (values[Matrix.MTRANS_X] > 0) {
                    values[Matrix.MTRANS_X] = 0;
                }
                if (values[Matrix.MTRANS_Y] > 0) {
                    values[Matrix.MTRANS_Y] = 0;
                }
                if (bottomright[0] < mScreenWidth) {
                    values[Matrix.MTRANS_X] += mScreenWidth - bottomright[0];
                }
                if (bottomright[1] < mScreenHeight) {
                    values[Matrix.MTRANS_Y] += mScreenHeight - bottomright[1];
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
