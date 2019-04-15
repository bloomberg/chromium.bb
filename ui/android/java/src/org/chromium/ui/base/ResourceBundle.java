// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;

import org.chromium.base.BuildConfig;
import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.util.Arrays;

/**
 * This class provides the resource bundle related methods for the native
 * library.
 */
@JNINamespace("ui")
final class ResourceBundle {
    private ResourceBundle() {}

    private static final String TAG = "ResourceBundle";

    /**
     * Return the location of a locale-specific uncompress .pak file asset.
     *
     * @param locale Chromium locale name.
     * @param inBundle If true, return the path of the uncompressed .pak file
     *                 containing Chromium UI strings within app bundles. If
     *                 false, return the path of the uncompressed WebView UI
     *                 strings instead. Note that APK .pak files are stored
     *                 compressed and handled differently.
     * @return Asset path to uncompressed .pak file, or null if the locale is
     *         not supported by this version of Chromium, or the file is
     *         missing.
     */
    @CalledByNative
    private static String getLocalePakResourcePath(String locale, boolean inBundle) {
        if (Arrays.binarySearch(BuildConfig.UNCOMPRESSED_LOCALES, locale) < 0) {
            // This locale is not supported by Chromium.
            return null;
        }
        String pathPrefix = "assets/stored-locales/";
        if (inBundle) {
            if (locale.equals("en-US")) {
                pathPrefix = "assets/fallback-locales/";
            } else {
                String lang = LocalizationUtils.getSplitLanguageForAndroid(
                        LocaleUtils.toLanguage(locale));
                pathPrefix = "assets/locales#lang_" + lang + "/";
            }
        }
        String assetPath = pathPrefix + locale + ".pak";
        AssetManager manager = ContextUtils.getApplicationContext().getAssets();
        try (AssetFileDescriptor afd = manager.openNonAssetFd(assetPath)) {
            return assetPath;
        } catch (IOException e) {
            // It makes sense to log here when the file exists, but is unable to be opened as an fd
            // because (for example) it is unexpectedly compressed in an apk. In that case, the log
            // message might save someone some time working out what has gone wrong.
            // For that reason, we only suppress the message when the exception message doesn't look
            // informative (Android framework passes the filename as the message on actual file not
            // found, and the empty string also wouldn't give any useful information for debugging)
            if (!e.getMessage().equals("") && !e.getMessage().equals(assetPath)) {
                Log.e(TAG, "Error while loading asset %s: %s", assetPath, e);
            }
        }
        return null;
    }
}
