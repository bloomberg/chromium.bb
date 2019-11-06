// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.support.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;

/** The mediator implements the logic to interact with the surfaces and caller. */
class StartSurfaceMediator implements StartSurface.Controller, TabSwitcher.OverviewModeObserver {
    private final ObserverList<StartSurface.OverviewModeObserver> mObservers = new ObserverList<>();
    private final TabSwitcher.Controller mController;
    @Nullable
    private final BottomBarCoordinator mBottomBarCoordinator;
    @Nullable
    private final ExploreSurfaceCoordinator mExploreSurfaceCoordinator;

    StartSurfaceMediator(TabSwitcher.Controller controller,
            @Nullable BottomBarCoordinator bottomBarCoordinator,
            @Nullable ExploreSurfaceCoordinator exploreSurfaceCoordinator) {
        mController = controller;
        mBottomBarCoordinator = bottomBarCoordinator;
        mExploreSurfaceCoordinator = exploreSurfaceCoordinator;

        if (mBottomBarCoordinator != null) {
            mBottomBarCoordinator.setOnClickListener(new BottomBarProperties.OnClickListener() {
                @Override
                public void onHomeButtonClicked() {
                    // TODO(crbug.com/982018): Show home surface and hide explore surface.
                }

                @Override
                public void onExploreButtonClicked() {
                    // TODO(crbug.com/982018): Show explore surface and hide home surface.
                }
            });
        }
        mController.addOverviewModeObserver(this);
    }

    // Implements StartSurface.Controller
    @Override
    public boolean overviewVisible() {
        return mController.overviewVisible();
    }

    @Override
    public void addOverviewModeObserver(StartSurface.OverviewModeObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeOverviewModeObserver(StartSurface.OverviewModeObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void hideOverview(boolean animate) {
        mController.hideOverview(animate);
    }

    @Override
    public void showOverview(boolean animate) {
        mController.showOverview(animate);

        // TODO(crbug.com/982018): Animate the bottom bar together with the Tab List view.
        if (mBottomBarCoordinator != null) mBottomBarCoordinator.setVisibility(true);
    }

    @Override
    public boolean onBackPressed() {
        // TODO(crbug.com/982018): Check whether explore surface is shown, if yes, switch back to
        // home surface.
        return mController.onBackPressed();
    }

    // Implements TabSwitcher.OverviewModeObserver.
    @Override
    public void startedShowing() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.startedShowing();
        }
    }

    @Override
    public void finishedShowing() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.finishedShowing();
        }
    }

    @Override
    public void startedHiding() {
        if (mBottomBarCoordinator != null) mBottomBarCoordinator.setVisibility(false);
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.startedHiding();
        }
    }

    @Override
    public void finishedHiding() {
        for (StartSurface.OverviewModeObserver observer : mObservers) {
            observer.finishedHiding();
        }
    }
}