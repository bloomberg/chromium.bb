// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.content.Context;
import android.graphics.PointF;
import android.text.InputType;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

import org.chromium.chromoting.jni.Client;

/**
 * The abstract class for viewing and interacting with a specific remote host. Handles logic
 * for touch input and render data.
 */
public abstract class DesktopView extends SurfaceView {
    /** Used to define the animation feedback shown when a user touches the screen. */
    public enum InputFeedbackType {
        NONE,
        SHORT_TOUCH_ANIMATION,
        LONG_TOUCH_ANIMATION,
        LONG_TRACKPAD_ANIMATION
    }

    protected final RenderData mRenderData;
    protected final TouchInputHandler mInputHandler;

    /**
     * Subclass should trigger this event when the client view size is changed.
     */
    protected final Event.Raisable<SizeChangedEventParameter> mOnClientSizeChanged =
            new Event.Raisable<>();

    /**
     * Subclass should trigger this event when the host (desktop frame) size is changed.
     */
    protected final Event.Raisable<SizeChangedEventParameter> mOnHostSizeChanged =
            new Event.Raisable<>();

    private final int mTinyFeedbackPixelRadius;
    private final int mSmallFeedbackPixelRadius;
    private final int mLargeFeedbackPixelRadius;

    /** The parent Desktop activity. */
    private final Desktop mDesktop;

    private final Event.Raisable<TouchEventParameter> mOnTouch = new Event.Raisable<>();

    public DesktopView(Desktop desktop, Client client) {
        super(desktop);
        Preconditions.notNull(desktop);
        Preconditions.notNull(client);
        mDesktop = desktop;
        mRenderData = new RenderData();
        mInputHandler = new TouchInputHandler(this, desktop, mRenderData);
        mInputHandler.init(desktop, new InputEventSender(client));

        // Give this view keyboard focus, allowing us to customize the soft keyboard's settings.
        setFocusableInTouchMode(true);

        mTinyFeedbackPixelRadius =
                getResources().getDimensionPixelSize(R.dimen.feedback_animation_radius_tiny);

        mSmallFeedbackPixelRadius =
                getResources().getDimensionPixelSize(R.dimen.feedback_animation_radius_small);

        mLargeFeedbackPixelRadius =
                getResources().getDimensionPixelSize(R.dimen.feedback_animation_radius_large);
    }

    // TODO(yuweih): move showActionBar and showKeyboard out of this abstract class.
    /** Shows the action bar. */
    public final void showActionBar() {
        mDesktop.showSystemUi();
    }

    /** Shows the software keyboard. */
    public final void showKeyboard() {
        InputMethodManager inputManager =
                (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        inputManager.showSoftInput(this, 0);
    }

    /** An {@link Event} which is triggered when user touches the screen. */
    public final Event<TouchEventParameter> onTouch() {
        return mOnTouch;
    }

    /** An {@link Event} which is triggered when the client size is changed. */
    public final Event<SizeChangedEventParameter> onClientSizeChanged() {
        return mOnClientSizeChanged;
    }

    /** An {@link Event} which is triggered when the host size is changed. */
    public final Event<SizeChangedEventParameter> onHostSizeChanged() {
        return mOnHostSizeChanged;
    }

    // View overrides.
    /** Called when a software keyboard is requested, and specifies its options. */
    @Override
    public final InputConnection onCreateInputConnection(EditorInfo outAttrs) {
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
    public final boolean onTouchEvent(MotionEvent event) {
        TouchEventParameter parameter = new TouchEventParameter(event);
        mOnTouch.raise(parameter);
        return parameter.handled;
    }

    /**
     * Returns the radius of the given feedback type.
     * 0.0f will be returned if no feedback should be shown.
     */
    protected final float getFeedbackRadius(InputFeedbackType feedbackToShow, float scaleFactor) {
        switch (feedbackToShow) {
            case NONE:
                return 0.0f;
            case SHORT_TOUCH_ANIMATION:
                return mSmallFeedbackPixelRadius / scaleFactor;
            case LONG_TOUCH_ANIMATION:
                return mLargeFeedbackPixelRadius / scaleFactor;
            case LONG_TRACKPAD_ANIMATION:
                // The size of the longpress trackpad animation is supposed to be close to the size
                // of the cursor so it doesn't need to be normalized and should be scaled with the
                // canvas.
                return mTinyFeedbackPixelRadius;
            default:
                // Unreachable, but required by Google Java style and findbugs.
                assert false : "Unreached";
                return 0.0f;
        }
    }

    /** Triggers a brief animation to indicate the existence and location of an input event. */
    public abstract void showInputFeedback(InputFeedbackType feedbackToShow, PointF pos);

    /**
     * Informs the view that its transformation matrix (for rendering the remote desktop bitmap)
     * has been changed by the TouchInputHandler, which requires repainting.
     */
    public abstract void transformationChanged();

    /**
     * Informs the view that the cursor has been moved by the TouchInputHandler, which requires
     * repainting.
     */
    public abstract void cursorMoved();

    /**
     * Informs the view that the cursor visibility has been changed (for different input mode) by
     * the TouchInputHandler, which requires repainting.
     */
    public abstract void cursorVisibilityChanged();

    /**
     * Starts or stops an animation. Whilst the animation is running, the DesktopView will
     * periodically call TouchInputHandler.processAnimation() and repaint itself.
     */
    public abstract void setAnimationEnabled(boolean enabled);
}
