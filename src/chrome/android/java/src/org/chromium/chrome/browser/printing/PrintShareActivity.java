// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import org.chromium.chrome.browser.ChromeAccessorActivity;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;

/**
 * A simple activity that allows Chrome to expose print as an option in the share menu.
 */
public class PrintShareActivity extends ChromeAccessorActivity {
    public static final String BROADCAST_ACTION = "PrintShareActivityBroadcastAction";

    @Override
    protected String getBroadcastAction() {
        return BROADCAST_ACTION;
    }

    public static boolean featureIsAvailable(Tab currentTab) {
        PrintingController printingController = PrintingControllerImpl.getInstance();
        return !currentTab.isNativePage() && !printingController.isBusy()
                && UserPrefs.get(Profile.getLastUsedRegularProfile())
                           .getBoolean(Pref.PRINTING_ENABLED);
    }
}
