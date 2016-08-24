// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.PointF;
import android.view.SurfaceHolder;

import org.chromium.chromoting.jni.Client;
import org.chromium.chromoting.jni.GlDisplay;

/**
 * The user interface for viewing and interacting with a specific remote host. Uses OpenGL to draw
 * the desktop and cursor. Should be used entirely on the UI thread.
 */
public class GlDesktopView extends DesktopView implements SurfaceHolder.Callback {
    private final GlDisplay mDisplay;

    private Object mOnHostSizeChangedListenerKey;
    private Object mOnCanvasRenderedListenerKey;

    private Event.ParameterRunnable<Void> mProcessAnimationRunnable;

    public GlDesktopView(GlDisplay display, Desktop desktop, Client client) {
        super(desktop, client);
        Preconditions.notNull(display);
        mDisplay = display;

        mProcessAnimationRunnable = new Event.ParameterRunnable<Void>() {
            @Override
            public void run(Void p) {
                mInputHandler.processAnimation();
            }
        };

        getHolder().addCallback(this);
    }

    @Override
    public void showInputFeedback(InputFeedbackType feedbackToShow, PointF pos) {
        float scaleFactor = mRenderData.transform.mapRadius(1);
        float diameter = getFeedbackRadius(feedbackToShow, scaleFactor) * 2.0f;
        if (diameter <= 0.0f) {
            return;
        }
        mDisplay.showCursorInputFeedback(pos.x, pos.y, diameter);
    }

    @Override
    public void transformationChanged() {
        if (mRenderData.imageHeight == 0 || mRenderData.imageWidth == 0
                || mRenderData.screenHeight == 0 || mRenderData.screenWidth == 0) {
            return;
        }
        float[] matrix = new float[9];
        mRenderData.transform.getValues(matrix);
        mDisplay.pixelTransformationChanged(matrix);
    }

    @Override
    public void cursorMoved() {
        PointF cursorPosition = mRenderData.getCursorPosition();
        mDisplay.cursorPixelPositionChanged(cursorPosition.x, cursorPosition.y);
    }

    @Override
    public void cursorVisibilityChanged() {
        mDisplay.cursorVisibilityChanged(mRenderData.drawCursor);
    }

    @Override
    public void setAnimationEnabled(boolean enabled) {
        if (enabled && mOnCanvasRenderedListenerKey == null) {
            mOnCanvasRenderedListenerKey = mDisplay.onCanvasRendered()
                    .add(mProcessAnimationRunnable);
            mInputHandler.processAnimation();
        } else if (!enabled && mOnCanvasRenderedListenerKey != null) {
            mDisplay.onCanvasRendered().remove(mOnCanvasRenderedListenerKey);
            mOnCanvasRenderedListenerKey = null;
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mOnHostSizeChangedListenerKey = mDisplay
                .onHostSizeChanged().add(new Event.ParameterRunnable<SizeChangedEventParameter>() {
                    @Override
                    public void run(SizeChangedEventParameter p) {
                        mRenderData.imageHeight = p.height;
                        mRenderData.imageWidth = p.width;

                        // Note that imageHeight and imageWidth must be set before
                        // mOnHostSizeChanged is triggered. mInputHandler expects the image size in
                        // mRenderData updated before its callback is called.
                        mOnHostSizeChanged.raise(p);
                    }
                });

        mDisplay.surfaceCreated(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        mRenderData.screenWidth = width;
        mRenderData.screenHeight = height;

        mDisplay.surfaceChanged(width, height);
        mOnClientSizeChanged.raise(new SizeChangedEventParameter(width, height));
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        // GlDisplay's life time spans to the whole session while GlDesktopView may be created and
        // destroyed for multiple times (say when the phone is rotated). It is important to remove
        // the listeners when the surface is about to be destroyed.
        if (mOnHostSizeChangedListenerKey != null) {
            mDisplay.onHostSizeChanged().remove(mOnHostSizeChangedListenerKey);
        }
        if (mOnCanvasRenderedListenerKey != null) {
            mDisplay.onCanvasRendered().remove(mOnCanvasRenderedListenerKey);
        }
        mDisplay.surfaceDestroyed();
    }
}
