// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * This interface defines the methods used to customize input handling for
 * a particular strategy.  The implementing class is responsible for sending
 * remote input events and defining implementation specific behavior.
 */
public interface InputStrategyInterface {
    /** Sends a pointer move event to the remote host. */
    void injectRemoteMoveEvent(int x, int y);

    /** Sends a pointer button event to the remote host. */
    void injectRemoteButtonEvent(int button, boolean pressed);

    /** Sends a scroll/pan event to the remote host. */
    void injectRemoteScrollEvent(int deltaX, int deltaY);

    /** Returns the feedback animation type to use for a short press. */
    DesktopView.InputFeedbackType getShortPressFeedbackType();

    /** Returns the feedback animation type to use for a long press. */
    DesktopView.InputFeedbackType getLongPressFeedbackType();

    /**
     * Indicates whether the view should be manipulated to keep the cursor in the center or if the
     * view and cursor are not linked.
     */
    boolean centerCursorInView();

    /** Indicates whether to invert cursor movement for panning/scrolling. */
    boolean invertCursorMovement();
}
