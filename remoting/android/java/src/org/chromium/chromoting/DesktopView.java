// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Point;
import android.os.Looper;
import android.os.SystemClock;
import android.text.InputType;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import org.chromium.base.Log;
import org.chromium.chromoting.jni.Client;
import org.chromium.chromoting.jni.Display;

/**
 * The user interface for viewing and interacting with a specific remote host.
 * It provides a canvas onto which the video feed is rendered, handles
 * multitouch pan and zoom gestures, and collects and forwards input events.
 */
/** GUI element that holds the drawing canvas. */
public class DesktopView extends SurfaceView implements DesktopViewInterface,
        SurfaceHolder.Callback {
    /** Used to define the animation feedback shown when a user touches the screen. */
    public enum InputFeedbackType { NONE, SMALL_ANIMATION, LARGE_ANIMATION }

    private static final String TAG = "Chromoting";

    private final RenderData mRenderData;
    private final TouchInputHandler mInputHandler;

    /** The parent Desktop activity. */
    private Desktop mDesktop;

    /** The Client connection, used to inject input and fetch the video frames. */
    private Client mClient;

    private Display mDisplay;


    // Flag to prevent multiple repaint requests from being backed up. Requests for repainting will
    // be dropped if this is already set to true. This is used by the main thread and the painting
    // thread, so the access should be synchronized on |mRenderData|.
    private boolean mRepaintPending;

    // Flag used to ensure that the SurfaceView is only painted between calls to surfaceCreated()
    // and surfaceDestroyed(). Accessed on main thread and display thread, so this should be
    // synchronized on |mRenderData|.
    private boolean mSurfaceCreated = false;

    private final Event.Raisable<PaintEventParameter> mOnPaint = new Event.Raisable<>();
    private final Event.Raisable<SizeChangedEventParameter> mOnClientSizeChanged =
            new Event.Raisable<>();
    private final Event.Raisable<SizeChangedEventParameter> mOnHostSizeChanged =
            new Event.Raisable<>();
    private final Event.Raisable<TouchEventParameter> mOnTouch = new Event.Raisable<>();

    // Variables to control animation by the TouchInputHandler.

    /** Protects mInputAnimationRunning. */
    private final Object mAnimationLock = new Object();

    /** Whether the TouchInputHandler has requested animation to be performed. */
    private boolean mInputAnimationRunning = false;

    public DesktopView(Context context, AttributeSet attributes) {
        super(context, attributes);

        // Give this view keyboard focus, allowing us to customize the soft keyboard's settings.
        setFocusableInTouchMode(true);

        mRenderData = new RenderData();
        mInputHandler = new TouchInputHandler(this, context, mRenderData);

        mRepaintPending = false;

        getHolder().addCallback(this);
    }

    @Override
    public void init(Desktop desktop, Client client) {
        Preconditions.isNull(mDesktop);
        Preconditions.isNull(mClient);
        Preconditions.isNull(mDisplay);
        Preconditions.notNull(desktop);
        Preconditions.notNull(client);
        Preconditions.notNull(client.getDisplay());
        Preconditions.isTrue(client.getDisplay() instanceof Display);
        mDesktop = desktop;
        mClient = client;
        mDisplay = (Display) client.getDisplay();
        mInputHandler.init(desktop, new InputEventSender(client));
    }

    public Event<PaintEventParameter> onPaint() {
        return mOnPaint;
    }

    public Event<SizeChangedEventParameter> onClientSizeChanged() {
        return mOnClientSizeChanged;
    }

    public Event<SizeChangedEventParameter> onHostSizeChanged() {
        return mOnHostSizeChanged;
    }

    public Event<TouchEventParameter> onTouch() {
        return mOnTouch;
    }

    /** Request repainting of the desktop view. */
    void requestRepaint() {
        synchronized (mRenderData) {
            if (mRepaintPending) {
                return;
            }
            mRepaintPending = true;
        }
        mDisplay.redrawGraphics();
    }

    /**
     * Redraws the canvas. This should be done on a non-UI thread or it could
     * cause the UI to lag. Specifically, it is currently invoked on the native
     * graphics thread using a JNI.
     */
    public void paint() {
        long startTimeMs = SystemClock.uptimeMillis();

        if (Looper.myLooper() == Looper.getMainLooper()) {
            Log.w(TAG, "Canvas being redrawn on UI thread");
        }

        Bitmap image = mDisplay.getVideoFrame();
        if (image == null) {
            // This can happen if the client is connected, but a complete video frame has not yet
            // been decoded.
            return;
        }

        int width = image.getWidth();
        int height = image.getHeight();
        boolean sizeChanged = false;
        synchronized (mRenderData) {
            if (mRenderData.imageWidth != width || mRenderData.imageHeight != height) {
                // TODO(lambroslambrou): Move this code into a sizeChanged() callback, to be
                // triggered from native code (on the display thread) when the remote screen size
                // changes.
                mRenderData.imageWidth = width;
                mRenderData.imageHeight = height;
                sizeChanged = true;
            }
        }
        if (sizeChanged) {
            mOnHostSizeChanged.raise(new SizeChangedEventParameter(width, height));
        }

        Canvas canvas;
        Point cursorPosition;
        boolean drawCursor;
        synchronized (mRenderData) {
            mRepaintPending = false;
            // Don't try to lock the canvas before it is ready, as the implementation of
            // lockCanvas() may throttle these calls to a slow rate in order to avoid consuming CPU.
            // Note that a successful call to lockCanvas() will prevent the framework from
            // destroying the Surface until it is unlocked.
            if (!mSurfaceCreated) {
                return;
            }
            canvas = getHolder().lockCanvas();
            if (canvas == null) {
                return;
            }
            canvas.setMatrix(mRenderData.transform);
            drawCursor = mRenderData.drawCursor;
            cursorPosition = mRenderData.getCursorPosition();
        }

        canvas.drawColor(Color.BLACK);
        canvas.drawBitmap(image, 0, 0, new Paint());

        float scaleFactor;
        synchronized (mRenderData) {
            scaleFactor = mRenderData.transform.mapRadius(1);
        }
        mOnPaint.raise(new PaintEventParameter(cursorPosition, canvas, scaleFactor));

        if (drawCursor) {
            Bitmap cursorBitmap = mDisplay.getCursorBitmap();
            if (cursorBitmap != null) {
                Point hotspot = mDisplay.getCursorHotspot();
                canvas.drawBitmap(cursorBitmap, cursorPosition.x - hotspot.x,
                        cursorPosition.y - hotspot.y, new Paint());
            }
        }

        getHolder().unlockCanvasAndPost(canvas);

        synchronized (mAnimationLock) {
            if (mInputAnimationRunning || !mOnPaint.isEmpty()) {
                getHandler().postAtTime(new Runnable() {
                    @Override
                    public void run() {
                        processAnimation();
                    }
                }, startTimeMs + 30);
            }
        }
    }

    private void processAnimation() {
        boolean running;
        synchronized (mAnimationLock) {
            running = mInputAnimationRunning;
        }
        if (running) {
            mInputHandler.processAnimation();
            requestRepaint();
        } else if (!mOnPaint.isEmpty()) {
            requestRepaint();
        }
    }

    /**
     * Called after the canvas is initially created, then after every subsequent resize, as when
     * the display is rotated.
     */
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        synchronized (mRenderData) {
            mRenderData.screenWidth = width;
            mRenderData.screenHeight = height;
        }

        attachRedrawCallback();
        mOnClientSizeChanged.raise(new SizeChangedEventParameter(width, height));
        requestRepaint();
    }

    public void attachRedrawCallback() {
        mDisplay.provideRedrawCallback(new Runnable() {
            @Override
            public void run() {
                paint();
            }
        });
    }

    /** Called when the canvas is first created. */
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        synchronized (mRenderData) {
            mSurfaceCreated = true;
        }
    }

    /**
     * Called when the canvas is finally destroyed. Marks the canvas as needing a redraw so that it
     * will not be blank if the user later switches back to our window.
     */
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        synchronized (mRenderData) {
            mSurfaceCreated = false;
        }
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
        TouchEventParameter parameter = new TouchEventParameter(event);
        mOnTouch.raise(parameter);
        return parameter.handled;
    }

    @Override
    public void showInputFeedback(InputFeedbackType feedbackToShow, Point pos) {
        if (feedbackToShow != InputFeedbackType.NONE) {
            FeedbackAnimator.startAnimation(this, pos, feedbackToShow);
            requestRepaint();
        }
    }

    @Override
    public void showActionBar() {
        mDesktop.showSystemUi();
    }

    @Override
    public void showKeyboard() {
        InputMethodManager inputManager =
                (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        inputManager.showSoftInput(this, 0);
    }

    @Override
    public void transformationChanged() {
        requestRepaint();
    }

    @Override
    public void cursorMoved() {
        // For current implementation, cursorMoved() is always followed by transformationChanged()
        // even if the canvas isn't really changed. For future we should improve this by not calling
        // transformationChanged() if the cursor is moved but the canvas is not changed.
    }

    @Override
    public void cursorVisibilityChanged() {
        requestRepaint();
    }

    @Override
    public void setAnimationEnabled(boolean enabled) {
        synchronized (mAnimationLock) {
            if (enabled && !mInputAnimationRunning) {
                requestRepaint();
            }
            mInputAnimationRunning = enabled;
        }
    }
}
