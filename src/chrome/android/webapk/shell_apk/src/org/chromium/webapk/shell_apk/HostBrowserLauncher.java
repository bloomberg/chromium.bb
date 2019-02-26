// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.ArrayList;

/** Contains methods for launching host browser. */
public class HostBrowserLauncher {
    private static final String TAG = "cr_HostBrowserLauncher";

    // Action for launching {@link WebappLauncherActivity}.
    // TODO(hanxi): crbug.com/737556. Replaces this string with the new WebAPK launch action after
    // it is propagated to all the Chrome's channels.
    public static final String ACTION_START_WEBAPK =
            "com.google.android.apps.chrome.webapps.WebappManager.ACTION_START_WEBAPP";

    // Must stay in sync with
    // {@link org.chromium.chrome.browser.ShortcutHelper#REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB}.
    private static final String REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB =
            "REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB";

    private static void grantUriPermissionToHostBrowser(
            Context context, Intent launchIntent, String hostBrowserPackageName) {
        ArrayList<Uri> uris = launchIntent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
        if (uris == null) {
            uris = new ArrayList<>();
            Uri uri = launchIntent.getParcelableExtra(Intent.EXTRA_STREAM);
            if (uri != null) {
                uris.add(uri);
            }
        }
        for (Uri uri : uris) {
            context.grantUriPermission(
                    hostBrowserPackageName, uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
    }

    /**
     * Launches host browser in WebAPK mode if the browser is WebAPK-compatible.
     * Otherwise, launches the host browser in tabbed mode.
     */
    public static void launch(Context context, HostBrowserLauncherParams params) {
        Log.v(TAG, "WebAPK Launch URL: " + params.getStartUrl());

        if (HostBrowserUtils.shouldLaunchInTab(params.getHostBrowserPackageName(),
                    params.getHostBrowserMajorChromiumVersion())) {
            launchInTab(context, params);
            return;
        }

        Intent launchIntent = createLaunchInWebApkModeIntent(context, params);
        if (params.getSelectedShareTargetActivityClassName() != null) {
            grantUriPermissionToHostBrowser(
                    context, launchIntent, params.getHostBrowserPackageName());
        }
        try {
            context.startActivity(launchIntent);
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "Unable to launch browser in WebAPK mode.");
            e.printStackTrace();
        }
    }

    /** Creates intent to launch host browser in WebAPK mode. */
    public static Intent createLaunchInWebApkModeIntent(
            Context context, HostBrowserLauncherParams params) {
        Intent intent = new Intent();
        intent.setAction(ACTION_START_WEBAPK);
        intent.setPackage(params.getHostBrowserPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Bundle copiedExtras = params.getOriginalIntent().getExtras();
        if (copiedExtras != null) {
            intent.putExtras(copiedExtras);
        }
        intent.putExtra(WebApkConstants.EXTRA_URL, params.getStartUrl())
                .putExtra(WebApkConstants.EXTRA_SOURCE, params.getSource())
                .putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, context.getPackageName())
                .putExtra(WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                        params.getSelectedShareTargetActivityClassName())
                .putExtra(WebApkConstants.EXTRA_FORCE_NAVIGATION, params.getForceNavigation());

        // Only pass on the start time if no user action was required between launching the webapk
        // and chrome starting up. See https://crbug.com/842023
        if (!params.wasDialogShown()) {
            intent.putExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, params.getLaunchTimeMs());
        }
        return intent;
    }

    /** Launches a WebAPK in its runtime host browser as a tab. */
    private static void launchInTab(Context context, HostBrowserLauncherParams params) {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(params.getStartUrl()));
        intent.setPackage(params.getHostBrowserPackageName());
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true)
                .putExtra(WebApkConstants.EXTRA_SOURCE, params.getSource());
        try {
            context.startActivity(intent);
        } catch (ActivityNotFoundException e) {
        }
    }
}
