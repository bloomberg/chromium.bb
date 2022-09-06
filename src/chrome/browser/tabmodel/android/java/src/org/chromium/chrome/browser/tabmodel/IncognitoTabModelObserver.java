// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

/**
 * An observer of {@link IncognitoTabModel} that receives events relevant to incognito tabs.
 */
public interface IncognitoTabModelObserver {
    /**
     * A delegate to control whether to show or hide the Incognito reauth dialog.
     */
    interface IncognitoReauthDialogDelegate {
        /**
         * An event which is fired to control the visibility of the Incognito re-authentication
         * dialog.
         *
         * @param newModel The new {@link TabModel} which is now active.
         * @param oldModel The previous {@link TabModel} which was active.
         */
        void onAfterTabModelSelected(TabModel newModel, TabModel oldModel);
    }

    /**
     * Called when the first tab of the {@link IncognitoTabModel} is created.
     */
    default void wasFirstTabCreated() {}

    /**
     * Called when the last tab of the {@link IncognitoTabModel} is closed.
     */
    default void didBecomeEmpty() {}
}
