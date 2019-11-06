// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.util.ConversionUtils;

/**
 * Centralizes UMA data collection for the Data Reduction Proxy.
 */
public class DataReductionProxyUma {
    public static final String UI_ACTION_HISTOGRAM_NAME = "DataReductionProxy.UIAction";

    public static final String USER_VIEWED_ORIGINAL_SIZE_HISTOGRAM_NAME =
            "DataReductionProxy.UserViewedOriginalSize";
    public static final String USER_VIEWED_SAVINGS_SIZE_HISTOGRAM_NAME =
            "DataReductionProxy.UserViewedSavingsSize";
    public static final String USER_VIEWED_USAGE_DIFFERENCE_HISTOGRAM_NAME =
            "DataReductionProxy.UserViewedUsageDifferenceWithBreakdown";
    public static final String USER_VIEWED_SAVINGS_DIFFERENCE_HISTOGRAM_NAME =
            "DataReductionProxy.UserViewedSavingsDifferenceWithBreakdown";

    // Represent the possible user actions in the various data reduction promos and settings menu.
    // This must remain in sync with DataReductionProxyUIAction in
    // tools/metrics/histograms/enums.xml.
    public static final int ACTION_ENABLED = 0;
    // The value of 1 is reserved for an iOS-specific action. Values 2, 3, 13, 14, and 15 are
    // deprecated actions.
    public static final int ACTION_DISMISSED = 4;
    public static final int ACTION_OFF_TO_OFF = 5;
    public static final int ACTION_OFF_TO_ON = 6;
    public static final int ACTION_ON_TO_OFF = 7;
    public static final int ACTION_ON_TO_ON = 8;
    public static final int ACTION_FRE_ENABLED = 9;
    public static final int ACTION_FRE_DISABLED = 10;
    public static final int ACTION_INFOBAR_ENABLED = 11;
    public static final int ACTION_INFOBAR_DISMISSED = 12;
    public static final int ACTION_MAIN_MENU_OFF_TO_OFF = 16;
    public static final int ACTION_MAIN_MENU_OFF_TO_ON = 17;
    public static final int ACTION_MAIN_MENU_ON_TO_OFF = 18;
    public static final int ACTION_MAIN_MENU_ON_TO_ON = 19;
    public static final int ACTION_STATS_RESET = 20;
    public static final int ACTION_MAIN_MENU_DISPLAYED_ON = 21;
    public static final int ACTION_MAIN_MENU_DISPLAYED_OFF = 22;
    public static final int ACTION_SITE_BREAKDOWN_DISPLAYED = 23;
    public static final int ACTION_SITE_BREAKDOWN_SORTED_BY_DATA_SAVED = 24;
    public static final int ACTION_SITE_BREAKDOWN_SORTED_BY_DATA_USED = 25;
    public static final int ACTION_SITE_BREAKDOWN_EXPANDED = 26;
    public static final int ACTION_SITE_BREAKDOWN_SORTED_BY_HOSTNAME = 27;
    public static final int ACTION_INFOBAR_OFF_TO_OFF = 28;
    public static final int ACTION_INFOBAR_OFF_TO_ON = 29;
    public static final int ACTION_INFOBAR_ON_TO_OFF = 30;
    public static final int ACTION_INFOBAR_ON_TO_ON = 31;
    public static final int ACTION_INDEX_BOUNDARY = 32;

    /**
     * Record the DataReductionProxy.UIAction histogram.
     * @param action User action at the promo, first run experience, or settings screen
     */
    public static void dataReductionProxyUIAction(int action) {
        assert action >= 0 && action < ACTION_INDEX_BOUNDARY;
        RecordHistogram.recordEnumeratedHistogram(
                UI_ACTION_HISTOGRAM_NAME, action, DataReductionProxyUma.ACTION_INDEX_BOUNDARY);
    }

    /**
     * Record UMA on data savings displayed to the user. Called when the user views the data
     * savings in the UI.
     * @param compressedTotalBytes The total data used as shown to the user.
     * @param originalTotalBytes Original total size as shown to the user.
     */
    public static void dataReductionProxyUserViewedSavings(
            long compressedTotalBytes, long originalTotalBytes) {
        // The byte counts are stored in KB. The largest histogram bucket is set to ~1 TB.
        RecordHistogram.recordCustomCountHistogram(USER_VIEWED_ORIGINAL_SIZE_HISTOGRAM_NAME,
                (int) ConversionUtils.bytesToKilobytes(originalTotalBytes), 1, 1000 * 1000 * 1000,
                100);
        RecordHistogram.recordCustomCountHistogram(USER_VIEWED_SAVINGS_SIZE_HISTOGRAM_NAME,
                (int) ConversionUtils.bytesToKilobytes(originalTotalBytes - compressedTotalBytes),
                1, 1000 * 1000 * 1000, 100);
    }

    /**
     * Record UMA on the difference between data savings displayed to the user and the sum of the
     * breakdown columns. Called when the user views the data savings in the UI.
     * @param savedDifference The percent difference of data saved in the range [0, 100].
     * @param usedDifference The percent difference of data used in the range [0, 100].
     */
    public static void dataReductionProxyUserViewedSavingsDifference(
            int savedDifference, int usedDifference) {
        RecordHistogram.recordPercentageHistogram(
                USER_VIEWED_USAGE_DIFFERENCE_HISTOGRAM_NAME, usedDifference);
        RecordHistogram.recordPercentageHistogram(
                USER_VIEWED_SAVINGS_DIFFERENCE_HISTOGRAM_NAME, savedDifference);
    }
}
