// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dependency_injection;

import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TrustedWebActivityCoordinator;
import org.chromium.chrome.browser.customtabs.CustomTabActivityLifecycleUmaTracker;
import org.chromium.chrome.browser.customtabs.CustomTabBottomBarDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.CustomTabTabPersistencePolicy;
import org.chromium.chrome.browser.customtabs.CustomTabTopBarDelegate;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityInitialPageLoader;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleCoordinator;
import org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleToolbarController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;

import dagger.Subcomponent;

/**
 * Activity-scoped component associated with
 * {@link org.chromium.chrome.browser.customtabs.CustomTabActivity}.
 */
@Subcomponent(modules = {ChromeActivityCommonsModule.class, CustomTabActivityModule.class})
@ActivityScope
public interface CustomTabActivityComponent extends ChromeActivityComponent {
    TrustedWebActivityCoordinator resolveTrustedWebActivityCoordinator();
    DynamicModuleToolbarController resolveDynamicModuleToolbarController();
    DynamicModuleCoordinator resolveDynamicModuleCoordinator();

    TabObserverRegistrar resolveTabObserverRegistrar();
    CustomTabTopBarDelegate resolveTobBarDelegate();
    CustomTabBottomBarDelegate resolveBottomBarDelegate();
    CustomTabActivityTabController resolveTabController();
    CustomTabActivityTabFactory resolveTabFactory();
    CustomTabActivityLifecycleUmaTracker resolveUmaTracker();
    CustomTabActivityInitialPageLoader resolveInitialPageLoader();
    CustomTabActivityNavigationController resolveNavigationController();
    CustomTabActivityTabProvider resolveTabProvider();
    CustomTabStatusBarColorProvider resolveCustomTabStatusBarColorProvider();

    CustomTabTabPersistencePolicy resolveTabPersistencePolicy(); // For testing
}
