// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Point;

import org.chromium.chromoting.jni.Client;

/**
 * Callback interface to allow the TouchInputHandler to request actions on the DesktopView.
 */
public interface DesktopViewInterface {
    /**
     * Initializes the instance. Implementations can assume this function will be called exactly
     * once after constructor but before other functions.
     */
    void init(Desktop desktop, Client client);

    /** Triggers a brief animation to indicate the existence and location of an input event. */
    void showInputFeedback(DesktopView.InputFeedbackType feedbackToShow, Point pos);

    /** Shows the action bar. */
    void showActionBar();

    /** Shows the software keyboard. */
    void showKeyboard();

    /**
     * Informs the view that its transformation matrix (for rendering the remote desktop bitmap)
     * has been changed by the TouchInputHandler, which requires repainting.
     */
    void transformationChanged();

    /**
     * Informs the view that the cursor has been moved by the TouchInputHandler, which requires
     * repainting.
     */
    void cursorMoved();

    /**
     * Informs the view that the cursor visibility has been changed (for different input mode) by
     * the TouchInputHandler, which requires repainting.
     */
    void cursorVisibilityChanged();

    /**
     * Starts or stops an animation. Whilst the animation is running, the DesktopView will
     * periodically call TouchInputHandler.processAnimation() and repaint itself.
     */
    void setAnimationEnabled(boolean enabled);

    /**
     * An {@link Event} which is triggered when the view is being painted. Adding handlers to this
     * event causes painting to be triggered continuously until they are all removed.
     */
    Event<PaintEventParameter> onPaint();

    /** An {@link Event} which is triggered when the client size is changed. */
    Event<SizeChangedEventParameter> onClientSizeChanged();

    /** An {@link Event} which is triggered when the host size is changed. */
    Event<SizeChangedEventParameter> onHostSizeChanged();

    /** An {@link Event} which is triggered when user touchs the screen. */
    Event<TouchEventParameter> onTouch();
}
