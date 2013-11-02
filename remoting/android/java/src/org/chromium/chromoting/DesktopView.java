// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.app.ActionBar;
import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Point;
import android.os.Looper;
import android.text.InputType;
import android.util.Log;
import android.view.inputmethod.InputMethodManager;
import android.view.MotionEvent;
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
public class DesktopView extends SurfaceView implements DesktopViewInterface, Runnable,
        SurfaceHolder.Callback {
    private RenderData mRenderData;
    private TouchInputHandler mInputHandler;
    private ActionBar mActionBar;

    // Flag to prevent multiple repaint requests from being backed up. Requests for repainting will
    // be dropped if this is already set to true. This is used by the main thread and the painting
    // thread, so the access should be synchronized on |mRenderData|.
    private boolean mRepaintPending;

    public DesktopView(Activity context) {
        super(context);

        // Give this view keyboard focus, allowing us to customize the soft keyboard's settings.
        setFocusableInTouchMode(true);

        mRenderData = new RenderData();
        mInputHandler = new TrackingInputHandler(this, context, mRenderData);
        mActionBar = context.getActionBar();
        mRepaintPending = false;

        getHolder().addCallback(this);
    }

    /**
     * Request repainting of the desktop view.
     */
    void requestRepaint() {
        synchronized (mRenderData) {
            if (mRepaintPending) {
                return;
            }
            mRepaintPending = true;
        }
        JniInterface.redrawGraphics();
    }

    /** Called whenever the screen configuration is changed. */
    public void onScreenConfigurationChanged() {
        mInputHandler.onScreenConfigurationChanged();
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

        Bitmap image = JniInterface.getVideoFrame();

        int width = image.getWidth();
        int height = image.getHeight();
        boolean sizeChanged = false;
        synchronized (mRenderData) {
            if (mRenderData.imageWidth != width || mRenderData.imageHeight != height) {
                // TODO(lambroslambrou): Move this code into a sizeChanged() callback, to be
                // triggered from JniInterface (on the display thread) when the remote screen size
                // changes.
                mRenderData.imageWidth = width;
                mRenderData.imageHeight = height;
                sizeChanged = true;
            }
        }
        if (sizeChanged) {
            mInputHandler.onHostSizeChanged(width, height);
        }

        Canvas canvas = getHolder().lockCanvas();
        synchronized (mRenderData) {
            mRepaintPending = false;
            canvas.setMatrix(mRenderData.transform);
        }

        canvas.drawColor(Color.BLACK);
        canvas.drawBitmap(image, 0, 0, new Paint());
        Bitmap cursorBitmap = JniInterface.getCursorBitmap();
        if (cursorBitmap != null) {
            Point hotspot = JniInterface.getCursorHotspot();
            int bitmapX, bitmapY;
            synchronized (mRenderData) {
                bitmapX = mRenderData.cursorPosition.x - hotspot.x;
                bitmapY = mRenderData.cursorPosition.y - hotspot.y;
            }
            canvas.drawBitmap(cursorBitmap, bitmapX, bitmapY, new Paint());
        }
        getHolder().unlockCanvasAndPost(canvas);
    }

    /**
     * Called after the canvas is initially created, then after every subsequent resize, as when
     * the display is rotated.
     */
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        mActionBar.hide();

        synchronized (mRenderData) {
            mRenderData.screenWidth = width;
            mRenderData.screenHeight = height;
        }

        mInputHandler.onClientSizeChanged(width, height);

        JniInterface.provideRedrawCallback(this);
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

    /** Called whenever the user attempts to touch the canvas. */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return mInputHandler.onTouchEvent(event);
    }

    @Override
    public void injectMouseEvent(int x, int y, int button, boolean pressed) {
        boolean cursorMoved = false;
        synchronized (mRenderData) {
            // Test if the cursor actually moved, which requires repainting the cursor. This
            // requires that the TouchInputHandler doesn't mutate |mRenderData.cursorPosition|
            // directly.
            if (x != mRenderData.cursorPosition.x) {
                mRenderData.cursorPosition.x = x;
                cursorMoved = true;
            }
            if (y != mRenderData.cursorPosition.y) {
                mRenderData.cursorPosition.y = y;
                cursorMoved = true;
            }
        }

        if (button == TouchInputHandler.BUTTON_UNDEFINED && !cursorMoved) {
            // No need to inject anything or repaint.
            return;
        }

        JniInterface.mouseAction(x, y, button, pressed);
        if (cursorMoved) {
            // TODO(lambroslambrou): Optimize this by only repainting the affected areas.
            requestRepaint();
        }
    }

    @Override
    public void showActionBar() {
        mActionBar.show();
    }

    @Override
    public void showKeyboard() {
        InputMethodManager inputManager =
                (InputMethodManager)getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        inputManager.showSoftInput(this, 0);
    }

    @Override
    public void transformationChanged() {
        requestRepaint();
    }
}
