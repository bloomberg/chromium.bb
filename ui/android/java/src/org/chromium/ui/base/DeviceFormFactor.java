// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Context;
import android.util.DisplayMetrics;

import org.chromium.base.annotations.CalledByNative;

/**
 * UI utilities for accessing form factor information.
 */
public class DeviceFormFactor {

    /**
     * The minimum width that would classify the device as a tablet or a large tablet.
     */
    public static final int MINIMUM_TABLET_WIDTH_DP = 600;
    private static final int MINIMUM_LARGE_TABLET_WIDTH_DP = 720;

    private static Boolean sIsTablet = null;
    private static Boolean sIsLargeTablet = null;

    /**
     * @param context Android's context.
     * @return        Whether the app should treat the device as a tablet for layout. This method
     *                does not depend on the current window size and is not affected by
     *                multi-window. It is dependent only on the device's size.
     */
    @CalledByNative
    public static boolean isTablet(Context context) {
        if (sIsTablet == null) {
            sIsTablet = getSmallestDeviceWidthDp(context.getResources().getDisplayMetrics())
                    >= MINIMUM_TABLET_WIDTH_DP;
        }
        return sIsTablet;
    }

    /**
     * @param context Android's context.
     * @return True if the current device's minimum dimension is larger than 720dp. This method
     *                does not depend on the current window size and is not affected by
     *                multi-window.
     */
    public static boolean isLargeTablet(Context context) {
        if (sIsLargeTablet == null) {
            sIsLargeTablet = getSmallestDeviceWidthDp(context.getResources().getDisplayMetrics())
                    >= MINIMUM_LARGE_TABLET_WIDTH_DP;
        }
        return sIsLargeTablet;
    }

    /**
     * Calculates the minimum device width in dp.
     * @param metrics The {@link DisplayMetrics} to use for calculating device size.
     * @return The smaller of device width and height in dp.
     */
    public static int getSmallestDeviceWidthDp(DisplayMetrics metrics) {
        int smallestDeviceWidthDp = Math.round(Math.min(metrics.heightPixels / metrics.density,
                metrics.widthPixels / metrics.density));
        return smallestDeviceWidthDp;
    }
}
