// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

/**
 * org.chromium.ui.base.TouchlessEventHandler
 */
public class TouchlessEventHandler {
    private static final String EVENT_HANDLER_INTERNAL =
            "org.chromium.ui.base.TouchlessEventHandlerInternal";
    private static TouchlessEventHandler sInstance;

    static {
        try {
            sInstance = (TouchlessEventHandler) Class.forName(EVENT_HANDLER_INTERNAL).newInstance();
        } catch (ClassNotFoundException | InstantiationException | IllegalAccessException
                | IllegalArgumentException e) {
            sInstance = null;
        }
    }

    public static boolean hasTouchlessEventHandler() {
        return sInstance != null;
    }

    public static boolean onUnconsumedKeyboardEventAck(int nativeCode) {
        // No null check is needed here because it called after hasTouchlessEventHandler in native.
        assert sInstance != null;
        return sInstance.onUnconsumedKeyboardEventAckInternal(nativeCode);
    }

    public static void addCursorObserver(CursorObserver observer) {
        if (sInstance != null) {
            sInstance.addCursorObserverInternal(observer);
        }
    }

    public static void removeCursorObserver(CursorObserver observer) {
        if (sInstance != null) {
            sInstance.removeCursorObserverInternal(observer);
        }
    }

    public static void onDidFinishNavigation() {
        if (sInstance != null) {
            sInstance.onDidFinishNavigationInternal();
        }
    }

    public static void onActivityHidden() {
        if (sInstance != null) {
            sInstance.onActivityHiddenInternal();
        }
    }

    public static void fallbackCursorModeLockCursor(
            boolean left, boolean right, boolean up, boolean down) {
        // No null check is needed here because it called after hasTouchlessEventHandler in native.
        sInstance.fallbackCursorModeLockCursorInternal(left, right, up, down);
    }

    public static void fallbackCursorModeSetCursorVisibility(boolean visible) {
        // No null check is needed here because it called after hasTouchlessEventHandler in native.
        sInstance.fallbackCursorModeSetCursorVisibilityInternal(visible);
    }

    protected boolean onUnconsumedKeyboardEventAckInternal(int nativeCode) {
        return false;
    }

    protected void addCursorObserverInternal(CursorObserver observer) {}

    protected void removeCursorObserverInternal(CursorObserver observer) {}

    protected void onDidFinishNavigationInternal() {}

    protected void onActivityHiddenInternal() {}

    protected void fallbackCursorModeLockCursorInternal(
            boolean left, boolean right, boolean up, boolean down) {}

    protected void fallbackCursorModeSetCursorVisibilityInternal(boolean visible) {}
}