// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.StrictModeContext;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.previews.PreviewsAndroidBridge;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.GURLUtils;

/**
 * This class contains helper methods for determining site settings availability and showing the
 * site settings page.
 */
public class SiteSettingsHelper {
    /**
     * Whether site settings is available for a given {@link WebContents}.
     * @param webContents The WebContents for which to check the site settings.
     */
    public static boolean isSiteSettingsAvailable(WebContents webContents) {
        boolean isOfflinePage = OfflinePageUtils.getOfflinePage(webContents) != null;
        boolean isPreviewPage =
                PreviewsAndroidBridge.getInstance().shouldShowPreviewUI(webContents);
        // TODO(crbug.com/1033178): dedupe the DomDistillerUrlUtils#getOriginalUrlFromDistillerUrl()
        // calls.
        String url = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(
                webContents.getVisibleUrlString());
        String scheme = GURLUtils.getScheme(url);
        return !isOfflinePage && !isPreviewPage
                && (UrlConstants.HTTP_SCHEME.equals(scheme)
                        || UrlConstants.HTTPS_SCHEME.equals(scheme));
    }

    /**
     * Shows the site settings activity for a given url.
     */
    public static void showSiteSettings(Context context, String fullUrl) {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent preferencesIntent = settingsLauncher.createSettingsActivityIntent(context,
                SingleWebsiteSettings.class.getName(),
                SingleWebsiteSettings.createFragmentArgsForSite(fullUrl));
        // Disabling StrictMode to avoid violations (https://crbug.com/819410).
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            context.startActivity(preferencesIntent);
        }
    }
}
