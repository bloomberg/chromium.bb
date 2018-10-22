// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.ArrayList;

/**
 * WebAPK's share handler Activity.
 */
public class ShareActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        long activityStartTime = SystemClock.elapsedRealtime();
        super.onCreate(savedInstanceState);

        String startUrl = extractShareTarget();
        if (TextUtils.isEmpty(startUrl)) {
            finish();
            return;
        }

        HostBrowserLauncher launcher = new HostBrowserLauncher(this, getIntent(), startUrl,
                WebApkConstants.SHORTCUT_SOURCE_SHARE, true /* forceNavigation */,
                activityStartTime);
        launcher.selectHostBrowserAndLaunch(() -> finish());
    }

    private String extractShareTarget() {
        ActivityInfo shareActivityInfo;
        try {
            shareActivityInfo = getPackageManager().getActivityInfo(
                    getComponentName(), PackageManager.GET_META_DATA);
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
                getIntent().getStringExtra(Intent.EXTRA_SUBJECT)));
        entryList.add(new Pair<>(metaData.getString(WebApkMetaDataKeys.SHARE_PARAM_TEXT),
                getIntent().getStringExtra(Intent.EXTRA_TEXT)));

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
}
