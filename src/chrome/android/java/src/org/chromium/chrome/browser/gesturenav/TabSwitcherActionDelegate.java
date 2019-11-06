// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Implementation of {@link NavigationHandler#ActionDelegate} that works for Tab switcher.
 * Swipe from left exits the tab switcher and goes back to the current tab. Can exit
 * Chrome app itself if there's no current tab.
 */
public class TabSwitcherActionDelegate implements NavigationHandler.ActionDelegate {
    private final Supplier<Tab> mCurrentTab;
    private final Runnable mBackPress;

    public TabSwitcherActionDelegate(Runnable backPress, Supplier<Tab> currentTab) {
        mBackPress = backPress;
        mCurrentTab = currentTab;
    }

    @Override
    public boolean canNavigate(boolean forward) {
        return !forward;
    }

    @Override
    public void navigate(boolean forward) {
        assert !forward : "Should be called only for back navigation";
        mBackPress.run();
    }

    @Override
    public boolean willBackExitApp() {
        return mCurrentTab.get() == null;
    }
}
