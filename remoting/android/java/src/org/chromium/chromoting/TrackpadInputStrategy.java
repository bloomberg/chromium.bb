// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Point;
import android.view.MotionEvent;

import org.chromium.chromoting.jni.JniInterface;

/**
 * Defines a set of behavior and methods to simulate trackpad behavior when responding to
 * local input event data.  This class is also responsible for forwarding input event data
 * to the remote host for injection there.
 */
public class TrackpadInputStrategy implements InputStrategyInterface {
    private final RenderData mRenderData;

    /** Mouse-button currently held down, or BUTTON_UNDEFINED otherwise. */
    private int mHeldButton = TouchInputHandlerInterface.BUTTON_UNDEFINED;

    public TrackpadInputStrategy(RenderData renderData) {
        mRenderData = renderData;

        synchronized (mRenderData) {
            mRenderData.drawCursor = true;
        }
    }

    @Override
    public boolean onTap(int button) {
        injectMouseButtonEvent(button, true);
        injectMouseButtonEvent(button, false);
        return true;
    }

    @Override
    public boolean onPressAndHold(int button) {
        injectMouseButtonEvent(button, true);
        mHeldButton = button;
        return true;
    }

    @Override
    public void onScroll(float distanceX, float distanceY) {
        JniInterface.sendMouseWheelEvent((int) -distanceX, (int) -distanceY);
    }

    @Override
    public void onMotionEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_UP
                && mHeldButton != TouchInputHandlerInterface.BUTTON_UNDEFINED) {
            injectMouseButtonEvent(mHeldButton, false);
            mHeldButton = TouchInputHandlerInterface.BUTTON_UNDEFINED;
        }
    }

    @Override
    public void injectCursorMoveEvent(int x, int y) {
        JniInterface.sendMouseEvent(x, y, TouchInputHandlerInterface.BUTTON_UNDEFINED, false);
    }

    @Override
    public DesktopView.InputFeedbackType getShortPressFeedbackType() {
        return DesktopView.InputFeedbackType.NONE;
    }

    @Override
    public DesktopView.InputFeedbackType getLongPressFeedbackType() {
        return DesktopView.InputFeedbackType.SMALL_ANIMATION;
    }

    @Override
    public boolean isIndirectInputMode() {
        return true;
    }

    private void injectMouseButtonEvent(int button, boolean pressed) {
        Point cursorPosition;
        synchronized (mRenderData) {
            cursorPosition = mRenderData.getCursorPosition();
        }
        JniInterface.sendMouseEvent(cursorPosition.x, cursorPosition.y, button, pressed);
    }
}
