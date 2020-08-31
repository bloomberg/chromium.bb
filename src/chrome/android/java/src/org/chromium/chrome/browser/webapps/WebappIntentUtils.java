// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Browser;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.HashSet;

/**
 * Utility methods for extracting data from Homescreen-shortcut/WebAPK launch intent.
 */
public class WebappIntentUtils {
    /**
     * PWA homescreen shortcut intent extras. Used for copying intent extras for
     * {@link WebappActivity} launch intent.
     */
    private static final String[] WEBAPP_INTENT_EXTRAS = new String[] {ShortcutHelper.EXTRA_ID,
            ShortcutHelper.EXTRA_URL, ShortcutHelper.EXTRA_FORCE_NAVIGATION,
            ShortcutHelper.EXTRA_SOURCE, ShortcutHelper.EXTRA_SCOPE, ShortcutHelper.EXTRA_ICON,
            ShortcutHelper.EXTRA_VERSION, ShortcutHelper.EXTRA_NAME,
            ShortcutHelper.EXTRA_SHORT_NAME, ShortcutHelper.EXTRA_DISPLAY_MODE,
            ShortcutHelper.EXTRA_ORIENTATION, ShortcutHelper.EXTRA_THEME_COLOR,
            ShortcutHelper.EXTRA_BACKGROUND_COLOR, ShortcutHelper.EXTRA_IS_ICON_GENERATED,
            ShortcutHelper.EXTRA_IS_ICON_ADAPTIVE};

    /**
     * WebAPK intent extras. Used for copying intent extras for {@link WebappActivity} launch
     * intent.
     */
    private static final String[] WEBAPK_INTENT_EXTRAS = new String[] {ShortcutHelper.EXTRA_ID,
            ShortcutHelper.EXTRA_URL, ShortcutHelper.EXTRA_FORCE_NAVIGATION,
            ShortcutHelper.EXTRA_SOURCE, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME,
            WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK,
            WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME,
            WebApkConstants.EXTRA_NEW_STYLE_SPLASH_SHOWN_TIME,
            WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
            Intent.EXTRA_SUBJECT, Intent.EXTRA_TEXT, Intent.EXTRA_STREAM,
            Browser.EXTRA_APPLICATION_ID};

    /**
     * Converts color from signed Integer where an unspecified color is represented as null to
     * to unsigned long where an unspecified color is represented as
     * {@link ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING}.
     */
    public static long colorFromIntegerColor(Integer color) {
        if (color != null) {
            return color.intValue();
        }
        return ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING;
    }

    public static boolean isLongColorValid(long longColor) {
        return (longColor != ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
    }

    /**
     * Converts color from unsigned long where an unspecified color is represented as
     * {@link ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING} to a signed Integer where an
     * unspecified color is represented as null.
     */
    public static Integer colorFromLongColor(long longColor) {
        return isLongColorValid(longColor) ? Integer.valueOf((int) longColor) : null;
    }

    public static String getId(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_ID);
    }

    public static String getUrl(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_URL);
    }

    public static String getWebApkPackageName(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);
    }

    /**
     * Returns the WebAPK's shell launch timestamp associated with the passed-in intent, or -1.
     */
    public static long getWebApkShellLaunchTime(Intent intent) {
        return intent.getLongExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, -1);
    }

    /**
     * Returns the timestamp when the WebAPK shell showed the splash screen. Returns -1 if the
     * WebAPK shell did not show the splash screen.
     */
    public static long getNewStyleWebApkSplashShownTime(Intent intent) {
        return intent.getLongExtra(WebApkConstants.EXTRA_NEW_STYLE_SPLASH_SHOWN_TIME, -1);
    }

    public static void copyWebappLaunchIntentExtras(Intent fromIntent, Intent toIntent) {
        copyIntentExtras(fromIntent, toIntent, WEBAPP_INTENT_EXTRAS);
    }

    public static void copyWebApkLaunchIntentExtras(Intent fromIntent, Intent toIntent) {
        copyIntentExtras(fromIntent, toIntent, WEBAPK_INTENT_EXTRAS);
    }

    /**
     * Copies intent extras.
     * @param fromIntent Intent to copy extras from.
     * @param toIntent Intent to copy extras to.
     * @param key Keys to be copied.
     */
    private static void copyIntentExtras(Intent fromIntent, Intent toIntent, String[] keys) {
        Bundle extras = fromIntent.getExtras();
        if (extras == null) return;

        // Make a copy of all of the intent extras and remove the ones we do not want to copy to
        // avoid dealing with the types of the extras.

        // Make a copy of Intent#keySet() to avoid modifying |fromIntent|.
        HashSet<String> extraKeysToRemove = new HashSet<>(extras.keySet());
        for (String key : keys) {
            extraKeysToRemove.remove(key);
        }
        for (String key : extraKeysToRemove) {
            extras.remove(key);
        }
        toIntent.putExtras(extras);
    }
}
