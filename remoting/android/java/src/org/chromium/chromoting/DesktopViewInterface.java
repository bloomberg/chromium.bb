// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * Callback interface to allow the TouchInputHandler to request actions on the DesktopView.
 */
public interface DesktopViewInterface {
    /** Inject a mouse-move event, with optional button press/release. */
    void injectMouseEvent(int x, int y, int button, boolean pressed);

    /** Shows the action bar. */
    void showActionBar();

    /** Shows the software keyboard. */
    void showKeyboard();

    /**
     * Informs the view that its transformation matrix (for rendering the remote desktop bitmap)
     * has been changed by the TouchInputHandler, which requires repainting.
     */
    void transformationChanged();
}
