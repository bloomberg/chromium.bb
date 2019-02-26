// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewStub;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.MenuButton;
import org.chromium.chrome.browser.toolbar.TabCountProvider;

/**
 * The coordinator for the tab switcher mode bottom toolbar. This class handles all interactions
 * that the tab switcher bottom toolbar has with the outside world.
 */
public class TabSwitcherBottomToolbarCoordinator {
    /** The mediator that handles events from outside the tab switcher bottom toolbar. */
    private final TabSwitcherBottomToolbarMediator mMediator;

    /** The color provider for tab switching mode. */
    private final TabSwitcherThemeColorProvider mTabSwitcherThemeColorProvider;

    /** The close all tabs button that lives in the tab switcher bottom bar. */
    private final CloseAllTabsButton mCloseAllTabsButton;

    /** The new tab button that lives in the tab switcher bottom toolbar. */
    private final BottomToolbarNewTabButton mNewTabButton;

    /** The menu button that lives in the tab switcher bottom toolbar. */
    private final MenuButton mMenuButton;

    /**
     * Build the coordinator that manages the tab switcher bottom toolbar.
     * @param stub The tab switcher bottom toolbar {@link ViewStub} to inflate.
     * @param incognitoStateProvider Notifies components when incognito mode is entered or exited.
     * @param newTabClickListener An {@link OnClickListener} that is triggered when the
     *                            new tab button is clicked.
     * @param closeTabsClickListener An {@link OnClickListener} that is triggered when the
     *                               close all tabs button is clicked.
     * @param menuButtonHelper An {@link AppMenuButtonHelper} that is triggered when the
     *                         menu button is clicked.
     * @param tabModelSelector A {@link TabModelSelector} that incognito toggle tab layout uses to
     *                         switch between normal and incognito tabs.
     * @param overviewModeBehavior The overview mode manager.
     * @param tabCountProvider Updates the tab count number in the tab switcher button and in the
     *                         incognito toggle tab layout.
     */
    public TabSwitcherBottomToolbarCoordinator(ViewStub stub,
            IncognitoStateProvider incognitoStateProvider, OnClickListener newTabClickListener,
            OnClickListener closeTabsClickListener, AppMenuButtonHelper menuButtonHelper,
            TabModelSelector tabModelSelector, OverviewModeBehavior overviewModeBehavior,
            TabCountProvider tabCountProvider) {
        final View root = stub.inflate();

        TabSwitcherBottomToolbarModel model = new TabSwitcherBottomToolbarModel();

        PropertyModelChangeProcessor.create(model, root, new TabSwitcherBottomToolbarViewBinder());

        mTabSwitcherThemeColorProvider =
                new TabSwitcherThemeColorProvider(ContextUtils.getApplicationContext());

        mTabSwitcherThemeColorProvider.setIncognitoStateProvider(incognitoStateProvider);

        mMediator = new TabSwitcherBottomToolbarMediator(
                model, mTabSwitcherThemeColorProvider, overviewModeBehavior);

        mCloseAllTabsButton = root.findViewById(R.id.close_all_tabs_button);
        mCloseAllTabsButton.setOnClickListener(closeTabsClickListener);
        mCloseAllTabsButton.setIncognitoStateProvider(incognitoStateProvider);
        mCloseAllTabsButton.setThemeColorProvider(mTabSwitcherThemeColorProvider);
        mCloseAllTabsButton.setTabCountProvider(tabCountProvider);

        mNewTabButton = root.findViewById(R.id.tab_switcher_new_tab_button);
        mNewTabButton.setOnClickListener(newTabClickListener);
        mNewTabButton.setIncognitoStateProvider(incognitoStateProvider);
        mNewTabButton.setThemeColorProvider(mTabSwitcherThemeColorProvider);

        mMenuButton = root.findViewById(R.id.menu_button_wrapper);
        mMenuButton.setThemeColorProvider(mTabSwitcherThemeColorProvider);
        mMenuButton.setTouchListener(menuButtonHelper);
        mMenuButton.setAccessibilityDelegate(menuButtonHelper);
    }

    /**
     * @return The tab switcher mode bottom toolbar's menu button.
     */
    public MenuButton getMenuButton() {
        return mMenuButton;
    }

    /**
     * Called when the accessibility enabled state changes.
     * @param enabled Whether accessibility is enabled.
     */
    public void onAccessibilityStatusChanged(boolean enabled) {
        mTabSwitcherThemeColorProvider.onAccessibilityStatusChanged(enabled);
    }

    /**
     * Clean up any state when the bottom toolbar is destroyed.
     */
    public void destroy() {
        mMediator.destroy();
        mTabSwitcherThemeColorProvider.destroy();
        mCloseAllTabsButton.destroy();
        mNewTabButton.destroy();
        mMenuButton.destroy();
    }
}
