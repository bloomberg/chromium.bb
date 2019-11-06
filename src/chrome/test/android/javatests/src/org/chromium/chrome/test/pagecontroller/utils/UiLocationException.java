// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import android.support.test.uiautomator.UiObject2;

/**
 * Exception class that represents an unexpected failure when trying to find
 * a UI element.
 */
public class UiLocationException extends IllegalStateException {
    private final UiObject2 mUiObject2;
    private final IUi2Locator mIUi2Locator;

    public UiObject2 getUiObject2() {
        return mUiObject2;
    }

    public IUi2Locator getIUi2Locator() {
        return mIUi2Locator;
    }

    private UiLocationException(
            String message, Throwable cause, UiObject2 mUiObject2, IUi2Locator mIUi2Locator) {
        super(message, cause);
        this.mUiObject2 = mUiObject2;
        this.mIUi2Locator = mIUi2Locator;
    }

    /**
     * Creates a UiLocationException exception.
     *
     * @param msg The error message.
     * @return    UiLocationException with msg.
     */
    public static UiLocationException newInstance(String msg) {
        return newInstance(msg, null, null);
    }

    /**
     * Creates a UiLocationException exception.
     *
     * @param msg     The error message.
     * @param locator The locator that failed to find any nodes.
     * @param root    The root that the locator searched under, or null if all the nodes were
     *                searched.
     * @return        UiLocationException with msg, locator, and root.
     */
    public static UiLocationException newInstance(String msg, IUi2Locator locator, UiObject2 root) {
        if (root == null) {
            return new UiLocationException(
                    msg + " Locator: " + locator + " on device", null, root, locator);
        } else {
            return new UiLocationException(
                    msg + " Locator: " + locator + " in " + root.toString(), null, root, locator);
        }
    }
}
