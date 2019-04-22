// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ZoomController;
import org.chromium.chrome.browser.accessibility.FontSizePrefs;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.touchless.TouchlessEventHandler;
import org.chromium.ui.touchless.TouchlessEventHandler.TouchlessZoomCallback;

/**
 * Enables zooming for all websites. Implements {@link TouchlessZoomCallback} and performs zoom in
 * and zoom out upon {@link TouchlessEventHandler}'s request.
 */
public class TouchlessZoomHelper
        implements TouchlessZoomCallback, ActivityTabProvider.ActivityTabObserver {
    private Tab mCurrentTab;
    private ActivityTabProvider mActivityTabProvider;

    public TouchlessZoomHelper(ActivityTabProvider tabProvider) {
        mActivityTabProvider = tabProvider;
        mActivityTabProvider.addObserverAndTrigger(this);
        FontSizePrefs.getInstance().enableTouchlessMode();
        TouchlessEventHandler.setZoomCallback(this);
    }

    @Override
    public void onZoomInRequested() {
        if (mCurrentTab != null) ZoomController.zoomIn(mCurrentTab.getWebContents());
    }

    @Override
    public void onZoomOutRequested() {
        if (mCurrentTab != null) ZoomController.zoomOut(mCurrentTab.getWebContents());
    }

    @Override
    public void onActivityTabChanged(Tab tab, boolean hint) {
        mCurrentTab = tab;
    }

    public void destroy() {
        TouchlessEventHandler.removeZoomCallback(this);
        mActivityTabProvider.removeObserver(this);
    }
}
