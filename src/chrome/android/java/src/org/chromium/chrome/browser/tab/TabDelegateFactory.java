// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;

/**
 * A factory class to create {@link Tab} related delegates.
 */
public class TabDelegateFactory {
    /**
     * Creates the {@link WebContentsDelegateAndroid} the tab will be initialized with.
     * @param tab The associated {@link Tab}.
     * @return The {@link WebContentsDelegateAndroid} to be used for this tab.
     */
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new TabWebContentsDelegateAndroid(tab);
    }

    /**
     * Creates the {@link ExternalNavigationHandler} the tab will use for its
     * {@link InterceptNavigationDelegate}.
     * @param tab The associated {@link Tab}.
     * @return The {@link ExternalNavigationHandler} to be used for this tab.
     */

    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        return new ExternalNavigationHandler(tab);
    }

    /**
     * Creates the {@link ContextMenuPopulator} the tab will be initialized with.
     * @param tab The associated {@link Tab}.
     * @return The {@link ContextMenuPopulator} to be used for this tab.
     */
    public ContextMenuPopulator createContextMenuPopulator(Tab tab) {
        return new ChromeContextMenuPopulator(new TabContextMenuItemDelegate(tab),
                ChromeContextMenuPopulator.ContextMenuMode.NORMAL);
    }

    /**
     * Return true if app banners are to be permitted in this tab. May need to be overridden.
     * @return true if app banners are permitted, and false otherwise.
     */
    public boolean canShowAppBanners() {
        return true;
    }

    /**
     * Creates the {@link BrowserControlsVisibilityDelegate} the tab will be initialized with.
     * @param tab The associated {@link Tab}.
     */
    public void createBrowserControlsState(Tab tab) {
        TabBrowserControlsState.create(tab, new TabStateBrowserControlsVisibilityDelegate(tab));
    }

    public TabDelegateFactory createNewTabDelegateFactory() {
        return new TabDelegateFactory();
    }
}
