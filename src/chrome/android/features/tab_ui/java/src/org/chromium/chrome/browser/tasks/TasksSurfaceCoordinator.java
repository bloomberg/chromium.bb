// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;

/**
 * Coordinator for displaying task-related surfaces (Tab Switcher, MV Tiles, Omnibox, etc.).
 *  Concrete implementation of {@link TasksSurface}.
 */
public class TasksSurfaceCoordinator implements TasksSurface {
    private final TabSwitcher mGridTabSwitcher;
    private final LinearLayout mLayout;
    private final FrameLayout mGridContainerLayout;

    public TasksSurfaceCoordinator(ChromeActivity activity) {
        mLayout = new LinearLayout(activity);
        mLayout.setLayoutParams(new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT));
        mLayout.setOrientation(LinearLayout.VERTICAL);

        mGridContainerLayout = new FrameLayout(activity);
        mGridContainerLayout.setLayoutParams(new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));

        mLayout.addView(mGridContainerLayout);

        activity.getCompositorViewHolder().addView(mLayout);

        mGridTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(
                activity, mGridContainerLayout);
    }

    @Override
    public void setOnTabSelectingListener(TabSwitcher.OnTabSelectingListener listener) {
        mGridTabSwitcher.setOnTabSelectingListener(listener);
    }

    @Override
    public TabSwitcher.Controller getController() {
        return mGridTabSwitcher.getController();
    }

    @Override
    public TabSwitcher.TabListDelegate getTabListDelegate() {
        return mGridTabSwitcher.getTabListDelegate();
    }
}
