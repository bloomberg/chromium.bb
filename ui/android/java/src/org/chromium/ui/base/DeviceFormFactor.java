// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Context;

import org.chromium.base.annotations.CalledByNative;

/**
 * UI utilities for accessing form factor information.
 */
public class DeviceFormFactor {

    /**
     * The minimum width that would classify the device as a tablet or a large tablet.
     */
    private static final int MINIMUM_TABLET_WIDTH_DP = 600;
    private static final int MINIMUM_LARGE_TABLET_WIDTH_DP = 720;

    private static Boolean sIsTablet = null;
    private static Boolean sIsLargeTablet = null;

    /**
     * @param context Android's context.
     * @return        Whether the app is should treat the device as a tablet for layout.
     */
    @CalledByNative
    public static boolean isTablet(Context context) {
        if (sIsTablet == null) {
            int minimumScreenWidthDp = context.getResources().getConfiguration()
                    .smallestScreenWidthDp;
            sIsTablet = minimumScreenWidthDp >= MINIMUM_TABLET_WIDTH_DP;
        }
        return sIsTablet;
    }

    /**
     * @return True if the current device's minimum dimension is larger than 720dp.
     */
    public static boolean isLargeTablet(Context context) {
        if (sIsLargeTablet == null) {
            int minimumScreenWidthDp = context.getResources().getConfiguration()
                    .smallestScreenWidthDp;
            sIsLargeTablet = minimumScreenWidthDp >= MINIMUM_LARGE_TABLET_WIDTH_DP;
        }
        return sIsLargeTablet;
    }

    /**
     * Resets form factor information. This method should be called due to a smallestScreenWidthDp
     * change in the {@link android.content.res.Configuration}.
     */
    public static void onConfigurationChanged() {
        sIsTablet = null;
        sIsLargeTablet = null;
    }

}
