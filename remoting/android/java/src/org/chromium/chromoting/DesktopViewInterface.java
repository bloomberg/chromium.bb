// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * Callback interface to allow the TouchInputHandler to request actions on the DesktopView.
 */
public interface DesktopViewInterface {
    /** Injects a mouse-move event, with optional button press/release. */
    void injectMouseEvent(int x, int y, int button, boolean pressed);

    /** Injects a mouse-wheel event with delta values. */
    void injectMouseWheelDeltaEvent(int deltaX, int deltaY);

    /** Triggers a brief cursor animation to indicate a long-press event. */
    void showLongPressFeedback();

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
     * Starts or stops an animation. Whilst the animation is running, the DesktopView will
     * periodically call TouchInputHandler.processAnimation() and repaint itself.
     */
    void setAnimationEnabled(boolean enabled);
}
