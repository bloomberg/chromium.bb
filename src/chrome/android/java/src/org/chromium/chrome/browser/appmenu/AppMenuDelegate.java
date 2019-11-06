// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.MenuItem;

/** A delegate to handle menu item selection. */
public interface AppMenuDelegate {
    /**
     * Called whenever an item in the app menu is selected.
     * See {@link android.app.Activity#onOptionsItemSelected(MenuItem)}.
     * @param itemId The id of the menu item that was selected.
     * @param menuItemData Extra data associated with the menu item. May be null.
     */
    boolean onOptionsItemSelected(int itemId, @Nullable Bundle menuItemData);

    /**
     * @return {@link AppMenuPropertiesDelegateImpl} instance that the {@link AppMenuHandlerImpl}
     *         should be using.
     */
    AppMenuPropertiesDelegate createAppMenuPropertiesDelegate();
}