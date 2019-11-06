// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.os.Bundle;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferenceUtils;

/**
 * Fragment to manage the Usage and crash reports preference and to explain to
 * the user what it does.
 */
public class UsageAndCrashReportsPreferenceFragment extends PreferenceFragmentCompat {
    private static final String PREF_USAGE_AND_CRASH_REPORTS_SWITCH =
            "usage_and_crash_reports_switch";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        PreferenceUtils.addPreferencesFromResource(this, R.xml.usage_and_crash_reports_preferences);
        getActivity().setTitle(R.string.usage_and_crash_reports_title_legacy);
        initUsageAndCrashReportsSwitch();
    }

    private void initUsageAndCrashReportsSwitch() {
        ChromeSwitchPreference usageAndCrashReportsSwitch =
                (ChromeSwitchPreference) findPreference(PREF_USAGE_AND_CRASH_REPORTS_SWITCH);
        boolean enabled =
                PrivacyPreferencesManager.getInstance().isUsageAndCrashReportingPermittedByUser();
        usageAndCrashReportsSwitch.setChecked(enabled);

        usageAndCrashReportsSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            UmaSessionStats.changeMetricsReportingConsent((boolean) newValue);
            return true;
        });

        usageAndCrashReportsSwitch.setManagedPreferenceDelegate(
                preference -> PrefServiceBridge.getInstance().isMetricsReportingManaged());
    }
}
