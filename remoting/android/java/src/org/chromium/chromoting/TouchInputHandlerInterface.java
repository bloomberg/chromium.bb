// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import org.chromium.chromoting.jni.Client;

/**
 * This interface allows multiple styles of touchscreen UI to be implemented and dynamically
 * switched. The DesktopView passes the low-level touchscreen events and other events via this
 * interface. The implementation recognizes and processes the touchscreen gestures, and then
 * performs actions on the DesktopView (such as panning/zooming the display, injecting input, or
 * showing/hiding UI elements). All methods are called on the main UI thread.
 */
public interface TouchInputHandlerInterface {
    // These constants must match those in the generated struct protocol::MouseEvent_MouseButton.
    int BUTTON_UNDEFINED = 0;
    int BUTTON_LEFT = 1;
    int BUTTON_MIDDLE = 2;
    int BUTTON_RIGHT = 3;

    /**
     * Initializes the instance. Implementations can assume this function will be called exactly
     * once after constructor but before other functions.
     */
    void init(Desktop desktop, Client client);

    /**
     * Whilst an animation is in progress, this method is called repeatedly until the animation is
     * cancelled. After this method returns, the DesktopView will schedule a repaint.
     */
    void processAnimation();
}
