// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.share.ShareActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.ui.widget.Toast;

/**
 * A simple activity that allows Chrome to expose send tab to self as an option in the share menu.
 */
public class SendTabToSelfShareActivity extends ShareActivity {
    @Override
    protected void handleShareAction(ChromeActivity triggeringActivity) {
        Tab tab = triggeringActivity.getActivityTabProvider().get();

        NavigationHistory history =
                tab.getWebContents().getNavigationController().getNavigationHistory();
        NavigationEntry entry = history.getEntryAtIndex(history.getCurrentEntryIndex());

        // TODO(crbug/946808) Add actual target device cache GUID.
        String targetDeviceSyncCacheGuid = "";
        SendTabToSelfAndroidBridge.addEntry(tab.getProfile(), entry.getUrl(), entry.getTitle(),
                entry.getTimestamp(), targetDeviceSyncCacheGuid);

        Toast.makeText(triggeringActivity, R.string.send_tab_to_self_toast, Toast.LENGTH_SHORT)
                .show();
    }

    public static boolean featureIsAvailable(Tab currentTab) {
        return SendTabToSelfAndroidBridge.isFeatureAvailable(currentTab.getWebContents());
    }
}
