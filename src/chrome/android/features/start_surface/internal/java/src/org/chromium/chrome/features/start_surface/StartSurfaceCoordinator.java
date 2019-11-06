// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.start_surface.R;

/**
 * Root coordinator that is responsible for showing start surfaces, like a grid of Tabs, explore
 * surface and the bottom bar to switch between them.
 */
public class StartSurfaceCoordinator implements StartSurface {
    private final TasksSurface mTasksSurface;
    private final StartSurfaceMediator mStartSurfaceMediator;
    private BottomBarCoordinator mBottomBarCoordinator;
    private ExploreSurfaceCoordinator mExploreSurfaceCoordinator;

    public StartSurfaceCoordinator(ChromeActivity activity) {
        mTasksSurface = TabManagementModuleProvider.getDelegate().createTasksSurface(activity);

        // Do not enable this feature when the bottom bar is enabled since it will
        // overlap the start surface's bottom bar.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TWO_PANES_START_SURFACE_ANDROID)
                && !FeatureUtilities.isBottomToolbarEnabled()) {
            // Margin the bottom of the Tab list to save space for the bottom bar.
            mTasksSurface.getTabListDelegate().setBottomControlsHeight(
                ContextUtils.getApplicationContext().getResources()
                    .getDimensionPixelSize(R.dimen.ss_bottom_bar_height));

            // Create the bottom bar.
            mBottomBarCoordinator = new BottomBarCoordinator(
                    activity, activity.getCompositorViewHolder(), activity.getTabModelSelector());

            // Create the explore surface.
            mExploreSurfaceCoordinator = new ExploreSurfaceCoordinator();
        }

        mStartSurfaceMediator = new StartSurfaceMediator(mTasksSurface.getController(),
                mBottomBarCoordinator, mExploreSurfaceCoordinator);
    }

    // Implements StartSurface.
    @Override
    public void setOnTabSelectingListener(StartSurface.OnTabSelectingListener listener) {
        mTasksSurface.setOnTabSelectingListener(listener);
    }

    @Override
    public Controller getController() {
        return mStartSurfaceMediator;
    }

    @Override
    public TabSwitcher.TabListDelegate getTabListDelegate() {
        return mTasksSurface.getTabListDelegate();
    }
}
