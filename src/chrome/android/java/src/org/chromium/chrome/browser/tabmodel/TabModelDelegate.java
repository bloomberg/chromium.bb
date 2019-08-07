// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;

/**
 * This class serves as a callback from TabModel to TabModelSelector. Avoid adding unnecessary
 * methods that expose too much access to TabModel. http://crbug.com/263579
 */
public interface TabModelDelegate {
    /**
     * Requests the specified to be shown.
     * @param tab The tab that is requested to be shown.
     * @param type The reason why this tab was requested to be shown.
     */
    void requestToShowTab(Tab tab, @TabSelectionType int type);

    /**
     * Delegate a request to close all tabs in a model.
     * @param incognito Whether the model is incognito.
     * @return Whether the request was handled.
     */
    boolean closeAllTabsRequest(boolean incognito);

    /**
     * @param model The specified model.
     * @return Whether the specified model is currently selected.
     */
    boolean isCurrentModel(TabModel model);

    // TODO(aurimas): clean these methods up.
    TabModel getCurrentModel();
    TabModel getModel(boolean incognito);
    boolean isInOverviewMode();
    boolean isSessionRestoreInProgress();
    void selectModel(boolean incognito);
}
