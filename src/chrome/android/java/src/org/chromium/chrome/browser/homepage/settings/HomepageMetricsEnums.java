// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage.settings;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains Homepage related enums used for metrics.
 */
public final class HomepageMetricsEnums {
    private HomepageMetricsEnums() {}

    /**
     * Possible states for HomeButton. Used for Histogram
     * Settings.ShowHomeButtonPreferenceStateManaged. Currently {@link
     * HomeButtonPreferenceState.MANAGED_DISABLED } is not used.
     *
     * These values are persisted to logs, and should therefore never be renumbered nor reused.
     */
    @IntDef({HomeButtonPreferenceState.USER_DISABLED, HomeButtonPreferenceState.USER_ENABLED,
            HomeButtonPreferenceState.MANAGED_DISABLED, HomeButtonPreferenceState.MANAGED_ENABLED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HomeButtonPreferenceState {
        int USER_DISABLED = 0;
        int USER_ENABLED = 1;
        int MANAGED_DISABLED = 2;
        int MANAGED_ENABLED = 3;

        int NUM_ENTRIES = 4;
    }

    /**
     * Possible location type for homepage. Used for Histogram "Settings.Homepage.LocationType"
     * recorded in {@link HomepageManager#recordHomepageLocationType()}.
     *
     * These values are persisted to logs, and should therefore never be renumbered nor reused.
     */
    @IntDef({HomepageLocationType.POLICY_NTP, HomepageLocationType.POLICY_OTHER,
            HomepageLocationType.PARTNER_PROVIDED_NTP, HomepageLocationType.PARTNER_PROVIDED_OTHER,
            HomepageLocationType.USER_CUSTOMIZED_NTP, HomepageLocationType.USER_CUSTOMIZED_OTHER,
            HomepageLocationType.DEFAULT_NTP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HomepageLocationType {
        int POLICY_NTP = 0;
        int POLICY_OTHER = 1;
        int PARTNER_PROVIDED_NTP = 2;
        int PARTNER_PROVIDED_OTHER = 3;
        int USER_CUSTOMIZED_NTP = 4;
        int USER_CUSTOMIZED_OTHER = 5;
        int DEFAULT_NTP = 6;

        int NUM_ENTRIES = 7;
    }
}
