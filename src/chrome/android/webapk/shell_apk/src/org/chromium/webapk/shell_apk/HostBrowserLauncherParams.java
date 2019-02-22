// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.ArrayList;

/** Convenience wrapper for parameters to {@link HostBrowserLauncher} methods. */
public class HostBrowserLauncherParams {
    private String mHostBrowserPackageName;
    private int mHostBrowserMajorChromiumVersion;
    private boolean mDialogShown;
    private Intent mOriginalIntent;
    private String mStartUrl;
    private int mSource;
    private boolean mForceNavigation;
    private long mLaunchTimeMs;

    /**
     * Constructs a HostBrowserLauncherParams object from the passed in Intent and from <meta-data>
     * in the Android Manifest.
     */
    public static HostBrowserLauncherParams createForIntent(Context context,
            ComponentName callingActivityComponentName, Intent intent,
            String hostBrowserPackageName, boolean dialogShown, long launchTimeMs) {
        Bundle metadata = WebApkUtils.readMetaData(context);
        if (metadata == null) return null;

        int hostBrowserMajorChromiumVersion = HostBrowserUtils.queryHostBrowserMajorChromiumVersion(
                context, hostBrowserPackageName);
        String startUrl = null;
        int source = WebApkConstants.SHORTCUT_SOURCE_UNKNOWN;
        boolean forceNavigation = false;
        if (Intent.ACTION_SEND.equals(intent.getAction())) {
            startUrl = computeStartUrlForShareTarget(context, callingActivityComponentName, intent);
            source = WebApkConstants.SHORTCUT_SOURCE_SHARE;
            forceNavigation = true;
        } else if (!TextUtils.isEmpty(intent.getDataString())) {
            startUrl = intent.getDataString();
            source = intent.getIntExtra(
                    WebApkConstants.EXTRA_SOURCE, WebApkConstants.SHORTCUT_SOURCE_EXTERNAL_INTENT);
            forceNavigation = intent.getBooleanExtra(WebApkConstants.EXTRA_FORCE_NAVIGATION, true);
        } else {
            startUrl = metadata.getString(WebApkMetaDataKeys.START_URL);
            source = WebApkConstants.SHORTCUT_SOURCE_UNKNOWN;
            forceNavigation = false;
        }

        if (startUrl == null) return null;

        startUrl = WebApkUtils.rewriteIntentUrlIfNecessary(startUrl, metadata);

        // Ignore deep links which came with non HTTP/HTTPS schemes and which were not rewritten.
        if (!doesUrlUseHttpOrHttpsScheme(startUrl)) return null;

        return new HostBrowserLauncherParams(hostBrowserPackageName,
                hostBrowserMajorChromiumVersion, dialogShown, intent, startUrl, source,
                forceNavigation, launchTimeMs);
    }

    /** Computes the start URL for the given share intent and share activity. */
    private static String computeStartUrlForShareTarget(
            Context context, ComponentName shareTargetComponentName, Intent intent) {
        ActivityInfo shareActivityInfo;
        try {
            shareActivityInfo = context.getPackageManager().getActivityInfo(
                    shareTargetComponentName, PackageManager.GET_META_DATA);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
        if (shareActivityInfo == null || shareActivityInfo.metaData == null) {
            return null;
        }
        Bundle metaData = shareActivityInfo.metaData;

        String shareAction = metaData.getString(WebApkMetaDataKeys.SHARE_ACTION);
        if (TextUtils.isEmpty(shareAction)) {
            return null;
        }

        // These can be null, they are checked downstream.
        ArrayList<Pair<String, String>> entryList = new ArrayList<>();
        entryList.add(new Pair<>(metaData.getString(WebApkMetaDataKeys.SHARE_PARAM_TITLE),
                intent.getStringExtra(Intent.EXTRA_SUBJECT)));
        entryList.add(new Pair<>(metaData.getString(WebApkMetaDataKeys.SHARE_PARAM_TEXT),
                intent.getStringExtra(Intent.EXTRA_TEXT)));

        return createWebShareTargetUriString(shareAction, entryList);
    }

    /**
     * Converts the action url and parameters of a webshare target into a URI.
     * Example:
     * - action = "https://example.org/includinator/share.html"
     * - params
     *     title param: "title"
     *     title intent: "news"
     *     text param: "description"
     *     text intent: "story"
     * Becomes:
     *   https://example.org/includinator/share.html?title=news&description=story
     * TODO(ckitagawa): The escaping behavior isn't entirely correct. The exact encoding is still
     * being discussed at https://github.com/WICG/web-share-target/issues/59.
     */
    protected static String createWebShareTargetUriString(
            String action, ArrayList<Pair<String, String>> entryList) {
        Uri.Builder queryBuilder = new Uri.Builder();
        for (Pair<String, String> nameValue : entryList) {
            if (!TextUtils.isEmpty(nameValue.first) && !TextUtils.isEmpty(nameValue.second)) {
                // Uri.Builder does URL escaping.
                queryBuilder.appendQueryParameter(nameValue.first, nameValue.second);
            }
        }
        Uri shareUri = Uri.parse(action);
        Uri.Builder builder = shareUri.buildUpon();
        // Uri.Builder uses %20 rather than + for spaces, the spec requires +.
        builder.encodedQuery(queryBuilder.build().toString().replace("%20", "+").substring(1));
        return builder.build().toString();
    }

    /** Returns whether the URL uses the HTTP or HTTPs schemes. */
    private static boolean doesUrlUseHttpOrHttpsScheme(String url) {
        return url != null && (url.startsWith("http:") || url.startsWith("https:"));
    }

    private HostBrowserLauncherParams(String hostBrowserPackageName,
            int hostBrowserMajorChromiumVersion, boolean dialogShown, Intent originalIntent,
            String startUrl, int source, boolean forceNavigation, long launchTimeMs) {
        mHostBrowserPackageName = hostBrowserPackageName;
        mHostBrowserMajorChromiumVersion = hostBrowserMajorChromiumVersion;
        mDialogShown = dialogShown;
        mOriginalIntent = originalIntent;
        mStartUrl = startUrl;
        mSource = source;
        mForceNavigation = forceNavigation;
        mLaunchTimeMs = launchTimeMs;
    }

    /** Returns the chosen host browser. */
    public String getHostBrowserPackageName() {
        return mHostBrowserPackageName;
    }

    /**
     * Returns the major version of the host browser. Currently, only Chromium host browsers
     * (Chrome Canary, Chrome Dev ...) are supported.
     */
    public int getHostBrowserMajorChromiumVersion() {
        return mHostBrowserMajorChromiumVersion;
    }

    /** Returns whether the choose-host-browser dialog was shown. */
    public boolean wasDialogShown() {
        return mDialogShown;
    }

    /** Returns intent used to launch WebAPK. */
    public Intent getOriginalIntent() {
        return mOriginalIntent;
    }

    /** Returns URL to launch WebAPK at. */
    public String getStartUrl() {
        return mStartUrl;
    }

    /** Returns the source which is launching/navigating the WebAPK. */
    public int getSource() {
        return mSource;
    }

    /**
     * Returns whether the WebAPK should be navigated to {@link mStartUrl} if it is already
     * running.
     */
    public boolean getForceNavigation() {
        return mForceNavigation;
    }

    /** Returns time in milliseconds that the WebAPK was launched. */
    public long getLaunchTimeMs() {
        return mLaunchTimeMs;
    }
}
