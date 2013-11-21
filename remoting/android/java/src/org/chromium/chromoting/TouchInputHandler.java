// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.view.MotionEvent;

/**
 * This interface allows multiple styles of touchscreen UI to be implemented and dynamically
 * switched. The DesktopView passes the low-level touchscreen events and other events via this
 * interface. The implementation recognizes and processes the touchscreen gestures, and then
 * performs actions on the DesktopView (such as panning/zooming the display, injecting input, or
 * showing/hiding UI elements). All methods are called on the main UI thread.
 */
public interface TouchInputHandler {
    // These constants must match those in the generated struct protoc::MouseEvent_MouseButton.
    int BUTTON_UNDEFINED = 0;
    int BUTTON_LEFT = 1;
    int BUTTON_MIDDLE = 2;
    int BUTTON_RIGHT = 3;

    /**
     * Processes a touch event. This should be called by the View in its onTouchEvent() handler.
     */
    boolean onTouchEvent(MotionEvent event);

    /**
     * Called when the screen configuration is changed, such as when the screen is rotated or an
     * external display is plugged in. This is not called if the client display area changes as a
     * result of showing/hiding UI elements such as a keyboard. For example, an implementation
     * could set a flag to reset the zoom level when the screen is rotated, but not when the
     * software keyboard appears. After this is called, the onClientSizeChanged() method will
     * shortly be called with the dimensions of the new client display area.
     */
    void onScreenConfigurationChanged();

    /**
     * Called whenever the client display area changes size. The caller will handle repainting
     * after this method returns.
     */
    void onClientSizeChanged(int width, int height);

    /**
     * Called when the host screen size is changed. The caller will handle repainting after this
     * method returns.
     */
    void onHostSizeChanged(int width, int height);

    /**
     * Whilst an animation is in progress, this method is called repeatedly until the animation is
     * cancelled. After this method returns, the DesktopView will schedule a repaint.
     */
    void processAnimation();
}
