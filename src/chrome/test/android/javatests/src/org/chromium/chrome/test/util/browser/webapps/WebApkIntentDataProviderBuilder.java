// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.webapps;

import android.graphics.Color;

import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.webapps.WebApkDistributor;
import org.chromium.chrome.browser.webapps.WebApkIntentDataProviderFactory;
import org.chromium.chrome.browser.webapps.WebDisplayMode;
import org.chromium.content_public.common.ScreenOrientationValues;

import java.util.ArrayList;
import java.util.HashMap;

/** Builder class for WebAPK {@link BrowserServicesIntentDataProvider} objects. */
public class WebApkIntentDataProviderBuilder {
    private String mWebApkPackageName;
    private String mUrl;
    private String mScope;
    private @WebDisplayMode int mDisplayMode = WebDisplayMode.STANDALONE;
    private String mManifestUrl;
    private int mWebApkVersionCode;

    public WebApkIntentDataProviderBuilder(String webApkPackageName, String url) {
        mWebApkPackageName = webApkPackageName;
        mUrl = url;
    }

    public void setScope(String scope) {
        mScope = scope;
    }

    public void setDisplayMode(@WebDisplayMode int displayMode) {
        mDisplayMode = displayMode;
    }

    public void setManifestUrl(String manifestUrl) {
        mManifestUrl = manifestUrl;
    }

    public void setWebApkVersionCode(int versionCode) {
        mWebApkVersionCode = versionCode;
    }

    /**
     * Builds {@link BrowserServicesIntentDataProvider} object using options that have been set.
     */
    public BrowserServicesIntentDataProvider build() {
        return WebApkIntentDataProviderFactory.create(mUrl, mScope, null, null, null, null,
                mDisplayMode, ScreenOrientationValues.DEFAULT, ShortcutSource.UNKNOWN,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING, Color.WHITE,
                false /* isPrimaryIconMaskable */, false /* isSplashIconMaskable */,
                mWebApkPackageName, /* shellApkVersion */ 1, mManifestUrl, mUrl,
                WebApkDistributor.BROWSER,
                new HashMap<String, String>() /* iconUrlToMurmur2HashMap */, null,
                false /* forceNavigation */, false /* isSplashProvidedByWebApk */, null,
                new ArrayList<>() /* shortcutItems */, mWebApkVersionCode);
    }
}
