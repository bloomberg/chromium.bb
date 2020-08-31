// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;

import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;

/** Layout wraps {@link StackLayout} to display the single start surface on top. */
public class StartSurfaceStackLayout extends StackLayout {
    private final StartSurfaceCoordinator mCoordinator;
    private final StartSurface.Controller mController;
    private boolean mIsInitialized;

    public StartSurfaceStackLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface) {
        super(context, updateHost, renderHost);

        mCoordinator = (StartSurfaceCoordinator) startSurface;
        mCoordinator.setOnTabSelectingListener(this::onTabSelecting);
        mController = mCoordinator.getController();
    }

    @Override
    public void onFinishNativeInitialization() {
        if (mIsInitialized) return;
        mIsInitialized = true;

        super.onFinishNativeInitialization();
        mCoordinator.initWithNative();
    }

    @Override
    public void show(long time, boolean animate) {
        // Lazy initialization if needed.
        mCoordinator.initialize();

        mController.showOverview(false);
        super.show(time, animate);
    }

    @Override
    public void startHiding(int nextId, boolean hintAtTabSelection) {
        mController.hideOverview(false);
        super.startHiding(nextId, hintAtTabSelection);
    }

    @Override
    public boolean onBackPressed() {
        if (mCoordinator.isShowingTabSwitcher()) {
            if (super.onBackPressed()) return true;
        }
        return mController.onBackPressed();
    }

    @Override
    protected EventFilter getEventFilter() {
        if (mCoordinator.isShowingTabSwitcher()) {
            return super.getEventFilter();
        }
        return null;
    }
}
