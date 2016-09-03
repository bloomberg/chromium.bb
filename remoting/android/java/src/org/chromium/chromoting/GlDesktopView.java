// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Matrix;
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

    public GlDesktopView(GlDisplay display, Desktop desktop, Client client) {
        super(desktop, client);
        Preconditions.notNull(display);
        mDisplay = display;
        display.setDesktopView(this);

        getHolder().addCallback(this);
    }

    @Override
    public void showInputFeedback(RenderStub.InputFeedbackType feedbackToShow, PointF pos) {
        mDisplay.showInputFeedback(feedbackToShow, pos);
    }

    @Override
    public void transformationChanged(Matrix matrix) {
        mDisplay.setTransformation(matrix);
    }

    @Override
    public void cursorMoved(PointF position) {
        mDisplay.moveCursor(position);
    }

    @Override
    public void cursorVisibilityChanged(boolean visible) {
        mDisplay.setCursorVisibility(visible);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mOnHostSizeChangedListenerKey = mDisplay
                .onHostSizeChanged().add(new Event.ParameterRunnable<SizeChangedEventParameter>() {
                    @Override
                    public void run(SizeChangedEventParameter p) {
                        mOnHostSizeChanged.raise(p);
                    }
                });

        mOnCanvasRenderedListenerKey = mDisplay
                .onCanvasRendered().add(new Event.ParameterRunnable<Void>() {
                    @Override
                    public void run(Void p) {
                        mOnCanvasRendered.raise(p);
                    }
                });

        mDisplay.surfaceCreated(holder);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        mDisplay.surfaceChanged(holder, format, width, height);
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
        mDisplay.surfaceDestroyed(holder);
    }
}
