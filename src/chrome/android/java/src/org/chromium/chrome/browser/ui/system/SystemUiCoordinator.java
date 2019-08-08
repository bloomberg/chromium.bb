// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.system;

import android.os.Build;
import android.support.annotation.Nullable;
import android.view.Window;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.ImmersiveModeManager;

/**
 * A UI coordinator that manages the system status bar and bottom navigation bar.
 *
 * TODO(https://crbug.com/943371): Move status bar code from Chrome*Activity into a new
 * StatusBarCoordinator.
 */
public class SystemUiCoordinator {
    private @Nullable NavigationBarColorController mNavigationBarColorController;

    /**
     * Construct a new {@link SystemUiCoordinator}.
     *
     * @param window The {@link Window} associated with the containing activity.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param immersiveModeManager The {@link ImmersiveModeManager} for the containing activity.
     * @param activityType The {@link org.chromium.chrome.browser.ChromeActivity.ActivityType} of
     *         the containing activity
     */
    public SystemUiCoordinator(Window window, TabModelSelector tabModelSelector,
            @Nullable ImmersiveModeManager immersiveModeManager,
            @ChromeActivity.ActivityType int activityType) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1
                && activityType == ChromeActivity.ActivityType.TABBED) {
            mNavigationBarColorController = new NavigationBarColorController(
                    window, tabModelSelector, immersiveModeManager);
        }
    }

    /**
     * Called when native initialization has finished to provide additional activity-scoped objects
     * only available after native initialization.
     *
     * @param overviewModeBehavior The {@link OverviewModeBehavior} for the containing activity
     *         if the current activity supports an overview mode, or null otherwise.
     */
    public void onNativeInitialized(@Nullable OverviewModeBehavior overviewModeBehavior) {
        if (mNavigationBarColorController != null) {
            assert overviewModeBehavior != null;

            mNavigationBarColorController.setOverviewModeBehavior(overviewModeBehavior);
        }
    }

    public void destroy() {
        if (mNavigationBarColorController != null) mNavigationBarColorController.destroy();
    }
}
