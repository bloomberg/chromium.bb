// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.LocaleUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.SuppressFBWarnings;

import java.io.File;
import java.util.ArrayList;
import java.util.Locale;

/**
 * This class provides the resource bundle related methods for the native
 * library.
 */
@JNINamespace("ui")
public class ResourceBundle {
    private static final String ASSET_DIR = "assets";
    private static final String FALLBACK_LOCALE = "en-US";
    private static String[] sActiveLocaleResources;

    /**
     * Initializes the list of locale packs for the active locale that are present within the apk.
     *
     * @param localePakFiles An array of pak filenames.
     */
    @SuppressFBWarnings("LI_LAZY_INIT_UPDATE_STATIC")  // Not thread-safe.
    public static void initializeLocalePaks(String[] localePakFiles) {
        ThreadUtils.assertOnUiThread();
        assert sActiveLocaleResources == null;
        String language = LocaleUtils.getLanguage(Locale.getDefault());
        ArrayList<String> activeLocalePakFiles = new ArrayList<String>(localePakFiles.length);
        for (String pakFileName : localePakFiles) {
            if (pakFileName.startsWith(language) || pakFileName.startsWith(FALLBACK_LOCALE)) {
                activeLocalePakFiles.add(pakFileName);
            }
        }
        sActiveLocaleResources = activeLocalePakFiles.toArray(new String[0]);
    }

    @SuppressFBWarnings("MS_EXPOSE_REP") // Don't modify the list.
    public static String[] getActiveLocaleResources() {
        return sActiveLocaleResources;
    }

    @CalledByNative
    private static String getLocalePakResourcePath(String locale) {
        if (sActiveLocaleResources == null) {
            return null;
        }
        String fileName = locale + ".pak";
        for (String resName : sActiveLocaleResources) {
            if (fileName.equals(resName)) {
                return new File(ASSET_DIR, resName).toString();
            }
        }
        return null;
    }
}
