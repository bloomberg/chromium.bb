// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilterFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.ui.base.WindowAndroid;

import javax.inject.Inject;

/**
 * Glue-level class that manages lifetime of root .tabmodel objects: {@link TabPersistentStore} and
 * {@link TabModelSelectorImpl} for custom tabs.
 */
@ActivityScope
public class CustomTabsTabModelOrchestrator extends TabModelOrchestrator {
    @Inject
    public CustomTabsTabModelOrchestrator() {}

    /**
     * Creates the TabModelSelector and the TabPersistentStore.
     */
    public void createTabModels(@Nullable Supplier<WindowAndroid> windowAndroidSupplier,
            TabCreatorManager tabCreatorManager, TabModelFilterFactory tabModelFilterFactory,
            TabPersistencePolicy persistencePolicy, AsyncTabParamsManager asyncTabParamsManager) {
        // Instantiate TabModelSelectorImpl
        NextTabPolicySupplier nextTabPolicySupplier = () -> NextTabPolicy.LOCATIONAL;
        mTabModelSelector = new TabModelSelectorImpl(windowAndroidSupplier, tabCreatorManager,
                tabModelFilterFactory, nextTabPolicySupplier, asyncTabParamsManager, false, false,
                false);

        // Instantiate TabPersistentStore
        mTabPersistentStore =
                new TabPersistentStore(persistencePolicy, mTabModelSelector, tabCreatorManager);

        wireSelectorAndStore();
        markTabModelsInitialized();
    }
}
