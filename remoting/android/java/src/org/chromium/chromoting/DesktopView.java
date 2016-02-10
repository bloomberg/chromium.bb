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
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.Shader;
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
    private final TouchInputHandlerInterface mInputHandler;

    /** The parent Desktop activity. */
    private Desktop mDesktop;

    /** The Client connection, used to inject input and fetch the video frames. */
    private Client mClient;


    // Flag to prevent multiple repaint requests from being backed up. Requests for repainting will
    // be dropped if this is already set to true. This is used by the main thread and the painting
    // thread, so the access should be synchronized on |mRenderData|.
    private boolean mRepaintPending;

    // Flag used to ensure that the SurfaceView is only painted between calls to surfaceCreated()
    // and surfaceDestroyed(). Accessed on main thread and display thread, so this should be
    // synchronized on |mRenderData|.
    private boolean mSurfaceCreated = false;

    /** Helper class for displaying the long-press feedback animation. This class is thread-safe. */
    private static class FeedbackAnimator {
        /** Total duration of the animation, in milliseconds. */
        private static final float TOTAL_DURATION_MS = 220;

        /** Start time of the animation, from {@link SystemClock#uptimeMillis()}. */
        private long mStartTimeInMs = 0;

        private boolean mRunning = false;

        /** Contains the size of the feedback animation for the most recent request. */
        private float mFeedbackSizeInPixels;

        /** Lock to allow multithreaded access to {@link #mStartTimeInMs} and {@link #mRunning}. */
        private final Object mLock = new Object();

        private Paint mPaint = new Paint();

        public boolean isAnimationRunning() {
            synchronized (mLock) {
                return mRunning;
            }
        }

        /**
         * Begins a new animation sequence. After calling this method, the caller should
         * call {@link #render(Canvas, float, float, float)} periodically whilst
         * {@link #isAnimationRunning()} returns true.
         */
        public void startAnimation(InputFeedbackType feedbackType) {
            if (feedbackType == InputFeedbackType.NONE) {
                return;
            }

            synchronized (mLock) {
                mRunning = true;
                mStartTimeInMs = SystemClock.uptimeMillis();
                mFeedbackSizeInPixels = getInputFeedbackSizeInPixels(feedbackType);
            }
        }

        public void render(Canvas canvas, float x, float y, float scale) {
            // |progress| is 0 at the beginning, 1 at the end.
            float progress;
            float size;
            synchronized (mLock) {
                // |mStartTimeInMs| is set and accessed on different threads (hence the lock).  It
                // is possible for |mStartTimeInMs| to be updated when an animation is in progress.
                // When this occurs, |radius| will eventually be set to 0 and used to initialize
                // RadialGradient which requires the radius to be > 0.  This will result in a crash.
                // In order to avoid this problem, we return early if the elapsed time is 0.
                float elapsedTimeInMs = SystemClock.uptimeMillis() - mStartTimeInMs;
                if (elapsedTimeInMs < 1) {
                    return;
                }

                progress = elapsedTimeInMs / TOTAL_DURATION_MS;
                if (progress >= 1) {
                    mRunning = false;
                    return;
                }
                size = mFeedbackSizeInPixels / scale;
            }

            // Animation grows from 0 to |size|, and goes from fully opaque to transparent for a
            // seamless fading-out effect. The animation needs to have more than one color so it's
            // visible over any background color.
            float radius = size * progress;
            int alpha = (int) ((1 - progress) * 0xff);

            int transparentBlack = Color.argb(0, 0, 0, 0);
            int white = Color.argb(alpha, 0xff, 0xff, 0xff);
            int black = Color.argb(alpha, 0, 0, 0);
            mPaint.setShader(new RadialGradient(x, y, radius,
                    new int[] {transparentBlack, white, black, transparentBlack},
                    new float[] {0.0f, 0.8f, 0.9f, 1.0f}, Shader.TileMode.CLAMP));
            canvas.drawCircle(x, y, radius, mPaint);
        }

        private float getInputFeedbackSizeInPixels(InputFeedbackType feedbackType) {
            switch (feedbackType) {
                case SMALL_ANIMATION:
                    return 40.0f;

                case LARGE_ANIMATION:
                    return 160.0f;

                default:
                    // Unreachable, but required by Google Java style and findbugs.
                    assert false : "Unreached";
                    return 0.0f;
            }
        }
    }

    private FeedbackAnimator mFeedbackAnimator = new FeedbackAnimator();

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

    public void setDesktop(Desktop desktop) {
        mDesktop = desktop;
    }

    public void setClient(Client client) {
        mClient = client;
    }

    /** See {@link TouchInputHandler#onSoftInputMethodVisibilityChanged} for API details. */
    public void onSoftInputMethodVisibilityChanged(boolean inputMethodVisible, Rect bounds) {
        mInputHandler.onSoftInputMethodVisibilityChanged(inputMethodVisible, bounds);
    }

    /** Request repainting of the desktop view. */
    void requestRepaint() {
        synchronized (mRenderData) {
            if (mRepaintPending) {
                return;
            }
            mRepaintPending = true;
        }
        mClient.redrawGraphics();
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

        Bitmap image = mClient.getVideoFrame();
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
            mInputHandler.onHostSizeChanged(width, height);
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

        // TODO(joedow): Replace the custom animation code with a standard Android implementation.
        boolean feedbackAnimationRunning = mFeedbackAnimator.isAnimationRunning();
        if (feedbackAnimationRunning) {
            float scaleFactor;
            synchronized (mRenderData) {
                scaleFactor = mRenderData.transform.mapRadius(1);
            }
            mFeedbackAnimator.render(canvas, cursorPosition.x, cursorPosition.y, scaleFactor);
        }

        if (drawCursor) {
            Bitmap cursorBitmap = mClient.getCursorBitmap();
            if (cursorBitmap != null) {
                Point hotspot = mClient.getCursorHotspot();
                canvas.drawBitmap(cursorBitmap, cursorPosition.x - hotspot.x,
                        cursorPosition.y - hotspot.y, new Paint());
            }
        }

        getHolder().unlockCanvasAndPost(canvas);

        synchronized (mAnimationLock) {
            if (mInputAnimationRunning || feedbackAnimationRunning) {
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
        }
        running |= mFeedbackAnimator.isAnimationRunning();
        if (running) {
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
        mInputHandler.onClientSizeChanged(width, height);
        requestRepaint();
    }

    public void attachRedrawCallback() {
        mClient.provideRedrawCallback(new Runnable() {
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
        return mInputHandler.onTouchEvent(event);
    }

    @Override
    public void showInputFeedback(InputFeedbackType feedbackToShow) {
        if (feedbackToShow != InputFeedbackType.NONE) {
            mFeedbackAnimator.startAnimation(feedbackToShow);
            requestRepaint();
        }
    }

    @Override
    public void showActionBar() {
        mDesktop.showActionBar();
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
    public void setAnimationEnabled(boolean enabled) {
        synchronized (mAnimationLock) {
            if (enabled && !mInputAnimationRunning) {
                requestRepaint();
            }
            mInputAnimationRunning = enabled;
        }
    }

    /** Updates the current InputStrategy used by the TouchInputHandler. */
    public void changeInputMode(
            Desktop.InputMode inputMode, CapabilityManager.HostCapability hostTouchCapability) {
        // We need both input mode and host input capabilities to select the input strategy.
        if (!inputMode.isSet() || !hostTouchCapability.isSet()) {
            return;
        }

        switch (inputMode) {
            case TRACKPAD:
                mInputHandler.setInputStrategy(new TrackpadInputStrategy(mRenderData, mClient));
                break;

            case TOUCH:
                if (hostTouchCapability.isSupported()) {
                    mInputHandler.setInputStrategy(new TouchInputStrategy(mRenderData, mClient));
                } else {
                    mInputHandler.setInputStrategy(
                            new SimulatedTouchInputStrategy(mRenderData, mClient, getContext()));
                }
                break;

            default:
                // Unreachable, but required by Google Java style and findbugs.
                assert false : "Unreached";
        }

        // Ensure the cursor state is updated appropriately.
        requestRepaint();
    }
}
