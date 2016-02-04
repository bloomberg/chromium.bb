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

    /**
     * @param context Android's context.
     * @return        Whether the app should treat the device as a tablet for layout.
     */
    @CalledByNative
    public static boolean isTablet(Context context) {
        int minimumScreenWidthDp = context.getResources().getConfiguration().smallestScreenWidthDp;
        return minimumScreenWidthDp >= MINIMUM_TABLET_WIDTH_DP;
    }

    /**
     * @return True if the current screen's minimum dimension is larger than 720dp.
     */
    public static boolean isLargeTablet(Context context) {
        int minimumScreenWidthDp = context.getResources().getConfiguration().smallestScreenWidthDp;
        return minimumScreenWidthDp >= MINIMUM_LARGE_TABLET_WIDTH_DP;
    }
}
