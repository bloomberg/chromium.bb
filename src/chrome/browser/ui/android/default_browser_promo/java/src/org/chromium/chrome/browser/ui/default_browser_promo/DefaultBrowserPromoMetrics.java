// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.default_browser_promo;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils.DefaultBrowserState;

/**
 * Helper class to record histograms related to the default browser promo.
 */
class DefaultBrowserPromoMetrics {
    /**
     * Record {@link DefaultBrowserState} when role manager dialog is shown.
     * @param currentState The {@link DefaultBrowserState} when the dialog is shown.
     */
    static void recordRoleManagerShow(@DefaultBrowserState int currentState) {
        assert currentState != DefaultBrowserState.CHROME_DEFAULT;
        RecordHistogram.recordEnumeratedHistogram("Android.DefaultBrowserPromo.RoleManagerShown",
                currentState, DefaultBrowserState.NUM_ENTRIES);
    }

    /**
     * Record the outcome of the default browser promo.
     * @param oldState The {@link DefaultBrowserState} when the dialog shown.
     * @param newState The {@link DefaultBrowserState} after user changes default.
     */
    static void recordOutcome(
            @DefaultBrowserState int oldState, @DefaultBrowserState int newState) {
        assert oldState != DefaultBrowserState.CHROME_DEFAULT;
        String name = oldState == DefaultBrowserState.NO_DEFAULT
                ? "Android.DefaultBrowserPromo.Outcome.NoDefault"
                : "Android.DefaultBrowserPromo.Outcome.OtherDefault";
        RecordHistogram.recordEnumeratedHistogram(name, newState, DefaultBrowserState.NUM_ENTRIES);
    }
}
