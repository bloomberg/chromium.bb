// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import dalvik.system.BaseDexClassLoader;

import org.chromium.base.annotations.CalledByNative;

/**
 * Utils for working with android app bundles.
 *
 * Important notes about bundle status as interpreted by this class:
 *
 * <ul>
 *   <li>If {@link BuildConfig#BUNDLES_SUPPORTED} is false, then we are definitely not in a bundle,
 *   and ProGuard is able to strip out the bundle support library.</li>
 *   <li>If {@link BuildConfig#BUNDLES_SUPPORTED} is true, then we MIGHT be in a bundle.
 *   {@link BundleUtils#sIsBundle} is the source of truth.</li>
 * </ul>
 *
 * We need two fields to store one bit of information here to ensure that ProGuard can optimize out
 * the bundle support library (since {@link BuildConfig#BUNDLES_SUPPORTED} is final) and so that
 * we can dynamically set whether or not we're in a bundle for targets that use static shared
 * library APKs.
 */
public final class BundleUtils {
    private static Boolean sIsBundle;

    /**
     * {@link BundleUtils#isBundle()}  is not called directly by native because
     * {@link CalledByNative} prevents inlining, causing the bundle support lib to not be
     * removed non-bundle builds.
     *
     * @return true if the current build is a bundle.
     */
    @CalledByNative
    public static boolean isBundleForNative() {
        return isBundle();
    }

    /**
     * @return true if the current build is a bundle.
     */
    public static boolean isBundle() {
        if (!BuildConfig.BUNDLES_SUPPORTED) {
            return false;
        }
        assert sIsBundle != null;
        return sIsBundle;
    }

    public static void setIsBundle(boolean isBundle) {
        sIsBundle = isBundle;
    }

    /* Returns absolute path to a native library in a feature module. */
    @CalledByNative
    private static String getNativeLibraryPath(String libraryName) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ((BaseDexClassLoader) ContextUtils.getApplicationContext().getClassLoader())
                    .findLibrary(libraryName);
        }
    }
}
