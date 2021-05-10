// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a
 * {@link TabModelSelector}.
 */
public class TabModelSelectorSupplier extends UnownedUserDataSupplier<TabModelSelector> {
    private static final UnownedUserDataKey<TabModelSelectorSupplier> KEY =
            new UnownedUserDataKey<TabModelSelectorSupplier>(TabModelSelectorSupplier.class);

    /** Return {@link TabModelSelector} supplier associated with the given {@link WindowAndroid}. */
    public static ObservableSupplier<TabModelSelector> from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /** Constructs a TabModelSelectorSupplier and attaches it to the {@link WindowAndroid} */
    public TabModelSelectorSupplier() {
        super(KEY);
    }
}