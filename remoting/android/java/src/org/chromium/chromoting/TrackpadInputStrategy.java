// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Point;
import android.view.MotionEvent;

/**
 * Defines a set of behavior and methods to simulate trackpad behavior when responding to
 * local input event data.  This class is also responsible for forwarding input event data
 * to the remote host for injection there.
 */
public class TrackpadInputStrategy implements InputStrategyInterface {
    private final RenderData mRenderData;
    private final InputEventSender mInjector;

    /** Mouse-button currently held down, or BUTTON_UNDEFINED otherwise. */
    private int mHeldButton = InputStub.BUTTON_UNDEFINED;

    public TrackpadInputStrategy(RenderData renderData, InputEventSender injector) {
        Preconditions.notNull(injector);
        mRenderData = renderData;
        mInjector = injector;

        synchronized (mRenderData) {
            mRenderData.drawCursor = true;
        }
    }

    @Override
    public boolean onTap(int button) {
        mInjector.sendMouseClick(getCursorPosition(), button);
        return true;
    }

    @Override
    public boolean onPressAndHold(int button) {
        mInjector.sendMouseDown(getCursorPosition(), button);
        mHeldButton = button;
        return true;
    }

    @Override
    public void onScroll(float distanceX, float distanceY) {
        mInjector.sendReverseMouseWheelEvent(distanceX, distanceY);
    }

    @Override
    public void onMotionEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_UP
                && mHeldButton != InputStub.BUTTON_UNDEFINED) {
            mInjector.sendMouseUp(getCursorPosition(), mHeldButton);
            mHeldButton = InputStub.BUTTON_UNDEFINED;
        }
    }

    @Override
    public void injectCursorMoveEvent(int x, int y) {
        mInjector.sendCursorMove(x, y);
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

    private Point getCursorPosition() {
        synchronized (mRenderData) {
            return mRenderData.getCursorPosition();
        }
    }
}
