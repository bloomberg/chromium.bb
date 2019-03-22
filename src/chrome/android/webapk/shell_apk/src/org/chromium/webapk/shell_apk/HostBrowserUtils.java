// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.webapk.lib.common.WebApkMetaDataKeys;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Contains methods for getting information about host browser.
 */
public class HostBrowserUtils {
    private static final int MINIMUM_REQUIRED_CHROME_VERSION = 57;

    private static final int MINIMUM_REQUIRED_INTENT_HELPER_VERSION = 2;

    private static final String TAG = "cr_HostBrowserUtils";

    /**
     * The package names of the browsers that support WebAPKs. The most preferred one comes first.
     */
    private static List<String> sBrowsersSupportingWebApk =
            new ArrayList<String>(Arrays.asList("com.google.android.apps.chrome",
                    "com.android.chrome", "com.chrome.beta", "com.chrome.dev", "com.chrome.canary",
                    "org.chromium.chrome", "org.chromium.arc.intent_helper"));

    /** Caches the package name of the host browser. */
    private static String sHostPackage;

    /** For testing only. */
    public static void resetCachedHostPackageForTesting() {
        sHostPackage = null;
    }

    /**
     * Returns a list of browsers that support WebAPKs. TODO(hanxi): Replace this function once we
     * figure out a better way to know which browser supports WebAPKs.
     */
    public static List<String> getBrowsersSupportingWebApk() {
        return sBrowsersSupportingWebApk;
    }

    /**
     * Returns the cached package name of the host browser to launch the WebAPK. Returns null if the
     * cached package name is no longer installed.
     */
    public static String getCachedHostBrowserPackage(Context context) {
        PackageManager packageManager = context.getPackageManager();
        if (WebApkUtils.isInstalled(packageManager, sHostPackage)) {
            return sHostPackage;
        }

        String hostPackage = getHostBrowserFromSharedPreference(context);
        if (!WebApkUtils.isInstalled(packageManager, hostPackage)) {
            hostPackage = null;
        }
        return hostPackage;
    }

    /**
     * Computes and returns the package name of the best host browser to launch the WebAPK. Returns
     * null if there is either no host browsers which support WebAPKs or if the user needs to
     * confirm the host browser selection. If the best host browser has changed, clears all of the
     * WebAPK's cached data.
     */
    public static String computeHostBrowserPackageClearCachedDataOnChange(Context context) {
        PackageManager packageManager = context.getPackageManager();
        if (WebApkUtils.isInstalled(packageManager, sHostPackage)) {
            return sHostPackage;
        }

        String hostInPreferences = getHostBrowserFromSharedPreference(context);
        sHostPackage = computeHostBrowserPackageNameInternal(context);
        if (!TextUtils.equals(sHostPackage, hostInPreferences)) {
            if (!TextUtils.isEmpty(hostInPreferences)) {
                deleteSharedPref(context);
                deleteInternalStorage(context);
            }
            writeHostBrowserToSharedPref(context, sHostPackage);
        }

        return sHostPackage;
    }

    /**
     * Writes the package name of the host browser to the SharedPreferences. If the host browser is
     * different than the previous one stored, delete the SharedPreference before storing the new
     * host browser.
     */
    public static void writeHostBrowserToSharedPref(Context context, String hostPackage) {
        if (TextUtils.isEmpty(hostPackage)) return;

        SharedPreferences.Editor editor = WebApkSharedPreferences.getPrefs(context).edit();
        editor.putString(WebApkSharedPreferences.PREF_RUNTIME_HOST, hostPackage);
        editor.apply();
    }

    /** Queries the given host browser's major version. */
    public static int queryHostBrowserMajorChromiumVersion(
            Context context, String hostBrowserPackageName) {
        PackageInfo info;
        try {
            info = context.getPackageManager().getPackageInfo(hostBrowserPackageName, 0);
        } catch (PackageManager.NameNotFoundException e) {
            return -1;
        }
        String versionName = info.versionName;
        int dotIndex = versionName.indexOf(".");
        if (dotIndex < 0) {
            return -1;
        }
        try {
            return Integer.parseInt(versionName.substring(0, dotIndex));
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    /** Returns whether a WebAPK should be launched as a tab. See crbug.com/772398. */
    public static boolean shouldLaunchInTab(
            String hostBrowserPackageName, int hostBrowserChromiumMajorVersion) {
        if (!sBrowsersSupportingWebApk.contains(hostBrowserPackageName)) {
            return true;
        }

        if (TextUtils.equals(hostBrowserPackageName, "org.chromium.arc.intent_helper")) {
            return hostBrowserChromiumMajorVersion < MINIMUM_REQUIRED_INTENT_HELPER_VERSION;
        }

        return hostBrowserChromiumMajorVersion < MINIMUM_REQUIRED_CHROME_VERSION;
    }

    /**
     * Returns the package name of the host browser to launch the WebAPK, or null if we did not find
     * one.
     */
    private static String computeHostBrowserPackageNameInternal(Context context) {
        PackageManager packageManager = context.getPackageManager();

        // Gets the package name of the host browser if it is stored in the SharedPreference.
        String cachedHostBrowser = getHostBrowserFromSharedPreference(context);
        if (!TextUtils.isEmpty(cachedHostBrowser)
                && WebApkUtils.isInstalled(packageManager, cachedHostBrowser)) {
            return cachedHostBrowser;
        }

        // Gets the package name of the host browser if it is specified in AndroidManifest.xml.
        String hostBrowserFromManifest =
                WebApkUtils.readMetaDataFromManifest(context, WebApkMetaDataKeys.RUNTIME_HOST);
        if (!TextUtils.isEmpty(hostBrowserFromManifest)) {
            if (WebApkUtils.isInstalled(packageManager, hostBrowserFromManifest)) {
                return hostBrowserFromManifest;
            }
            return null;
        }

        // Gets the package name of the default browser on the Android device.
        // TODO(hanxi): Investigate the best way to know which browser supports WebAPKs.
        String defaultBrowser = getDefaultBrowserPackageName(context.getPackageManager());
        if (!TextUtils.isEmpty(defaultBrowser) && sBrowsersSupportingWebApk.contains(defaultBrowser)
                && WebApkUtils.isInstalled(packageManager, defaultBrowser)) {
            return defaultBrowser;
        }

        // If there is only one browser supporting WebAPK, and we can't decide which browser to use
        // by looking up cache, metadata and default browser, open with that browser.
        int availableBrowserCounter = 0;
        String lastSupportedBrowser = null;
        for (String packageName : sBrowsersSupportingWebApk) {
            if (availableBrowserCounter > 1) break;
            if (WebApkUtils.isInstalled(packageManager, packageName)) {
                availableBrowserCounter++;
                lastSupportedBrowser = packageName;
            }
        }
        if (availableBrowserCounter == 1) {
            return lastSupportedBrowser;
        }
        return null;
    }

    /** Returns the package name of the host browser cached in the SharedPreferences. */
    public static String getHostBrowserFromSharedPreference(Context context) {
        SharedPreferences sharedPref = WebApkSharedPreferences.getPrefs(context);
        return sharedPref.getString(WebApkSharedPreferences.PREF_RUNTIME_HOST, null);
    }

    /** Returns the package name of the default browser on the Android device. */
    private static String getDefaultBrowserPackageName(PackageManager packageManager) {
        Intent browserIntent = WebApkUtils.getQueryInstalledBrowsersIntent();
        ResolveInfo resolveInfo =
                packageManager.resolveActivity(browserIntent, PackageManager.MATCH_DEFAULT_ONLY);
        if (resolveInfo == null || resolveInfo.activityInfo == null) return null;

        return resolveInfo.activityInfo.packageName;
    }

    /** Deletes the SharedPreferences for the given context. */
    private static void deleteSharedPref(Context context) {
        SharedPreferences.Editor editor = WebApkSharedPreferences.getPrefs(context).edit();
        editor.clear();
        editor.apply();
    }

    /** Deletes the internal storage for the given context. */
    private static void deleteInternalStorage(Context context) {
        DexLoader.deletePath(context.getCacheDir());
        DexLoader.deletePath(context.getFilesDir());
        DexLoader.deletePath(
                context.getDir(HostBrowserClassLoader.DEX_DIR_NAME, Context.MODE_PRIVATE));
    }
}
