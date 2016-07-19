// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Point;
import android.view.SurfaceHolder;

import org.chromium.chromoting.jni.Client;
import org.chromium.chromoting.jni.GlDisplay;

/**
 * The user interface for viewing and interacting with a specific remote host. Uses OpenGL to draw
 * the desktop and cursor. Should be used entirely on the UI thread.
 */
public class GlDesktopView extends AbstractDesktopView implements SurfaceHolder.Callback {
    private final GlDisplay mDisplay;

    private Object mOnHostSizeChangedListenerKey;
    private Object mOnCanvasRenderedListenerKey;

    public GlDesktopView(GlDisplay display, Desktop desktop, Client client) {
        super(desktop, client);
        Preconditions.notNull(display);
        mDisplay = display;

        getHolder().addCallback(this);
    }

    @Override
    public void showInputFeedback(InputFeedbackType feedbackToShow, Point pos) {
        float diameter;
        switch (feedbackToShow) {
            case LARGE_ANIMATION:
                diameter = 320.f;
                break;
            case SMALL_ANIMATION:
                diameter = 80.f;
                break;
            default:
                return;
        }
        float scaleFactor = mRenderData.transform.mapRadius(1);
        mDisplay.showCursorInputFeedback(pos.x, pos.y, diameter / scaleFactor);
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
        Point cursorPosition = mRenderData.getCursorPosition();
        mDisplay.cursorPixelPositionChanged(cursorPosition.x, cursorPosition.y);
    }

    @Override
    public void cursorVisibilityChanged() {
        mDisplay.cursorVisibilityChanged(mRenderData.drawCursor);
    }

    @Override
    public void setAnimationEnabled(boolean enabled) {
        mDisplay.setRenderEventEnabled(enabled);
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

        mOnCanvasRenderedListenerKey = mDisplay
                .onCanvasRendered().add(new Event.ParameterRunnable<Void>() {
                    @Override
                    public void run(Void p) {
                        getHandler().post(new Runnable() {
                            @Override
                            public void run() {
                                mInputHandler.processAnimation();
                            }
                        });
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
        mDisplay.onHostSizeChanged().remove(mOnHostSizeChangedListenerKey);
        mDisplay.onCanvasRendered().remove(mOnCanvasRenderedListenerKey);
        mDisplay.surfaceDestroyed();
    }
}
