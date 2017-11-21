// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Context;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.WindowManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.ui.R;

/**
 * UI utilities for accessing form factor information.
 */
public class DeviceFormFactor {
    /**
     * The minimum width that would classify the device as a tablet or a large tablet.
     */
    public static final int MINIMUM_TABLET_WIDTH_DP = 600;

    /**
     * @return Whether the app should treat the device as a tablet for layout. This method is not
     *         affected by Android N multi-window.
     */
    @CalledByNative
    public static boolean isTablet() {
        // On some devices, OEM modifications have been made to the resource loader that cause the
        // DeviceFormFactor calculation of whether a device is using tablet resources to be
        // incorrect. Check which resources were actually loaded rather than look at screen size.
        // See crbug.com/662338.
        return ContextUtils.getApplicationContext().getResources().getBoolean(R.bool.is_tablet);
    }

    /**
     * @param context {@link Context} used to get the Application Context.
     * @return True if the app should treat the device as a large (> 720dp) tablet for layout. This
     * method is not affected by Android N multi-window.
     */
    public static boolean isLargeTablet(Context context) {
        return ContextUtils.getApplicationContext().getResources().getBoolean(
                R.bool.is_large_tablet);
    }

    /**
     * Calculates the minimum device width in dp. This method is not affected by Android N
     * multi-window.
     *
     * @return The smaller of device width and height in dp.
     */
    public static int getSmallestDeviceWidthDp() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            DisplayMetrics metrics = new DisplayMetrics();
            // The Application Context must be used instead of the regular Context, because
            // in Android N multi-window calling Display.getRealMetrics() using the regular Context
            // returns the size of the current screen rather than the device.
            ((WindowManager) ContextUtils.getApplicationContext().getSystemService(
                     Context.WINDOW_SERVICE))
                    .getDefaultDisplay()
                    .getRealMetrics(metrics);
            return Math.round(Math.min(metrics.heightPixels / metrics.density,
                    metrics.widthPixels / metrics.density));
        } else {
            // Display.getRealMetrics() is only available in API level 17+, so
            // Configuration.smallestScreenWidthDp is used instead. Proir to the introduction of
            // multi-window in Android N, smallestScreenWidthDp was the same as the minimum size
            // in getRealMetrics().
            return ContextUtils.getApplicationContext()
                    .getResources()
                    .getConfiguration()
                    .smallestScreenWidthDp;
        }
    }

    /**
     * @param context {@link Context} used to get the display density.
     * @return The minimum width in px at which the device should be treated like a tablet for
     *         layout.
     */
    public static int getMinimumTabletWidthPx(Context context) {
        return Math.round(
                MINIMUM_TABLET_WIDTH_DP * context.getResources().getDisplayMetrics().density);
    }
}