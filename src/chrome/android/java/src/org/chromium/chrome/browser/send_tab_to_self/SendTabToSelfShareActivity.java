// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeAccessorActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfCoordinator;
import org.chromium.chrome.browser.sync.AndroidSyncSettings;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.ui.base.WindowAndroid;

/**
 * A simple activity that allows Chrome to expose send tab to self as an option in the share menu.
 */
public class SendTabToSelfShareActivity extends ChromeAccessorActivity {
    private static BottomSheetController sBottomSheetControllerForTesting;
    private static AndroidSyncSettings sAndroidSyncSettings;

    @Override
    public void handleAction(ChromeActivity triggeringActivity) {
        Tab tab = triggeringActivity.getActivityTabProvider().get();
        if (tab == null) return;
        NavigationEntry entry = tab.getWebContents().getNavigationController().getVisibleEntry();
        if (entry == null) return;
        BottomSheetController controller =
                getBottomSheetController(triggeringActivity.getWindowAndroid());
        if (controller == null) {
            return;
        }

        boolean isSyncEnabled = getAndroidSyncSettings().isSyncEnabled();
        controller.requestShowContent(
                SendTabToSelfCoordinator.createBottomSheetContent(triggeringActivity,
                        entry.getUrl(), entry.getTitle(), entry.getTimestamp(), controller,
                        new SettingsLauncherImpl(), isSyncEnabled),
                true);
        // TODO(crbug.com/968246): Remove the need to call this explicitly and instead have it
        // automatically show since PeekStateEnabled is set to false.
        controller.expandSheet();
    }

    public static boolean featureIsAvailable(Tab currentTab) {
        return SendTabToSelfAndroidBridge.isFeatureAvailable(currentTab.getWebContents());
    }

    private BottomSheetController getBottomSheetController(WindowAndroid window) {
        if (sBottomSheetControllerForTesting != null) return sBottomSheetControllerForTesting;
        return BottomSheetControllerProvider.from(window);
    }

    private AndroidSyncSettings getAndroidSyncSettings() {
        if (sAndroidSyncSettings != null) return sAndroidSyncSettings;
        return AndroidSyncSettings.get();
    }

    @VisibleForTesting
    public static void setBottomSheetControllerForTesting(BottomSheetController controller) {
        sBottomSheetControllerForTesting = controller;
    }

    @VisibleForTesting
    public static void setAndroidSyncSettingsForTesting(AndroidSyncSettings syncSettings) {
        sAndroidSyncSettings = syncSettings;
    }
}
