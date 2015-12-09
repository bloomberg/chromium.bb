// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Point;

import org.chromium.chromoting.jni.JniInterface;

/**
 * Defines a set of behavior and methods to simulate trackpad behavior when responding to
 * local input event data.  This class is also responsible for forwarding input event data
 * to the remote host for injection there.
 */
public class TrackpadInputStrategy implements InputStrategyInterface {
    private final RenderData mRenderData;
    private final DesktopViewInterface mViewer;

    public TrackpadInputStrategy(DesktopViewInterface viewer, RenderData renderData) {
        mViewer = viewer;
        mRenderData = renderData;
    }

    @Override
    public void injectRemoteMoveEvent(int x, int y) {
        injectRemoteButtonEvent(x, y, TouchInputHandlerInterface.BUTTON_UNDEFINED, false);
    }

    @Override
    public void injectRemoteButtonEvent(int button, boolean pressed) {
        Point cursorPosition;
        synchronized (mRenderData) {
            cursorPosition = mRenderData.getCursorPosition();
        }

        injectRemoteButtonEvent(cursorPosition.x, cursorPosition.y, button, pressed);
    }

    @Override
    public void injectRemoteScrollEvent(int deltaX, int deltaY) {
        JniInterface.sendMouseWheelEvent(deltaX, deltaY);
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
    public boolean centerCursorInView() {
        return true;
    }

    @Override
    public boolean invertCursorMovement() {
        return true;
    }

    private void injectRemoteButtonEvent(int x, int y, int button, boolean pressed) {
        boolean cursorMoved;
        synchronized (mRenderData) {
            cursorMoved = mRenderData.setCursorPosition(x, y);
        }

        if (button == TouchInputHandlerInterface.BUTTON_UNDEFINED && !cursorMoved) {
            // No need to inject anything or repaint.
            return;
        }

        JniInterface.sendMouseEvent(x, y, button, pressed);
        if (cursorMoved) {
            mViewer.transformationChanged();
        }
    }
}
