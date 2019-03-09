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
        return sInstance.onUnconsumedKeyboardEventAckInternal(nativeCode);
    }

    protected boolean onUnconsumedKeyboardEventAckInternal(int nativeCode) {
        return false;
    }
}