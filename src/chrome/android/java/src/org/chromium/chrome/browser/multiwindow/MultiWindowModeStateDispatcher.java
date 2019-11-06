// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.os.Build;

/**
 * Monitors multi-window mode state changes in the associated activity and dispatches changes
 * to registered observers.
 * TODO(crbug.com/931494): Set up observer registration.
 */
public class MultiWindowModeStateDispatcher {
    // TODO(crbug.com/931494): Reference to activity shouldn't be necessary once signals are wired
    // from ChromeActivity into this class.
    private final Activity mActivity;

    /**
     * Construct a new MultiWindowModeStateDispatcher.
     * @param activity The activity associated with this state dispatcher.
     */
    public MultiWindowModeStateDispatcher(Activity activity) {
        mActivity = activity;
    }

    /**
     * {See @link Activity#isInMultiWindowMode}.
     * @return Whether the activity associated with this state dispatcher is currently in
     *         multi-window mode.
     */
    public boolean isInMultiWindowMode() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return false;

        return mActivity.isInMultiWindowMode();
    }

    /**
     * See {@link MultiWindowUtils#isOpenInOtherWindowSupported(Activity)}.
     * @return Whether open in other window is supported for the activity associated with this
     *         state dispatcher.
     */
    public boolean isOpenInOtherWindowSupported() {
        return MultiWindowUtils.getInstance().isOpenInOtherWindowSupported(mActivity);
    }
}
