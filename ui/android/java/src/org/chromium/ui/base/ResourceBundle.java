// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.BuildConfig;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.File;
import java.util.Arrays;

/**
 * This class provides the resource bundle related methods for the native
 * library.
 */
@JNINamespace("ui")
public class ResourceBundle {
    private static final String ASSET_DIR = "assets";

    @CalledByNative
    private static String getLocalePakResourcePath(String locale) {
        String fileName = locale + ".pak";
        if (Arrays.binarySearch(BuildConfig.UNCOMPRESSED_ASSETS, fileName) >= 0) {
            return new File(ASSET_DIR, fileName).toString();
        }
        return null;
    }
}
