// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;

/**
 * Coordinator responsible for enabling or disabling the soft keyboard.
 */
class AssistantKeyboardCoordinator {
    private final ChromeActivity mActivity;
    private final ActivityKeyboardVisibilityDelegate mKeyboardDelegate;
    private final KeyboardVisibilityListener mKeyboardVisibilityListener =
            this::onKeyboardVisibilityChanged;
    private boolean mAllowShowingSoftKeyboard = true;

    AssistantKeyboardCoordinator(ChromeActivity activity) {
        mActivity = activity;
        mKeyboardDelegate = activity.getWindowAndroid().getKeyboardDelegate();
    }

    /**
     * Enable or disable the soft keyboard.
     */
    public void allowShowingSoftKeyboard(boolean allowed) {
        mAllowShowingSoftKeyboard = allowed;
        if (!allowed) {
            mKeyboardDelegate.hideKeyboard(mActivity.getCompositorViewHolder());
        }
    }

    /**
     * Start or stop listening for keyboard visibility changes.
     */
    public void enableListenForKeyboardVisibility(boolean enabled) {
        if (enabled) {
            mKeyboardDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        } else {
            mKeyboardDelegate.removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        }
    }

    // TODO(crbug.com/806868): Current solution only hides the keyboard once it has been already
    // shown. We should improve it and prevent from the showing in the first place.
    private void onKeyboardVisibilityChanged(boolean isShowing) {
        if (isShowing && !mAllowShowingSoftKeyboard) {
            mKeyboardDelegate.hideKeyboard(mActivity.getCompositorViewHolder());
        }
    }
}
