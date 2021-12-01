// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import androidx.annotation.AnyThread;
import androidx.annotation.MainThread;

import org.chromium.base.CommandLine;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

import java.util.Random;

/**
 * Creates a Field Trial to control the MobileIdentityConsistencyFRE feature. This feature is used
 * to show a new First Run page that will let users sign into chrome without syncing. The trial is
 * client controlled because this experiment runs on First Run Experience when native code is not
 * initialized and variation seed in not available.
 *
 * After creating a field trial the group information is saved in {@link SharedPreferencesManager}
 * so that it's available in subsequent runs of Chrome.
 */
public class FREMobileIdentityConsistencyFieldTrial {
    private static final Object LOCK = new Object();
    private static final String ENABLED_GROUP = "Enabled";
    private static final String DISABLED_GROUP = "Disabled";
    private static final String DEFAULT_GROUP = "Default";

    @AnyThread
    public static boolean isEnabled() {
        // Switch used for tests. Disabled by default otherwise.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE)) {
            return false;
        }
        return CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE)
                || ENABLED_GROUP.equals(getFirstRunTrialGroup());
    }

    @CalledByNative
    @AnyThread
    public static String getFirstRunTrialGroup() {
        synchronized (LOCK) {
            return SharedPreferencesManager.getInstance().readString(
                    ChromePreferenceKeys.FIRST_RUN_FIELD_TRIAL_GROUP, DEFAULT_GROUP);
        }
    }

    /**
     * This method should be only called once during FRE in
     * {@link org.chromium.chrome.browser.firstrun.FirstRunActivity} so that subsequent chrome runs
     * don't override FRE experiment group information.
     *
     * FRE is launched either after first install of chrome or after a power wash. So there is be no
     * previous experiment group information available when this method is called.
     *
     * The group information is registered as a synthetic field trial in native code inside
     * ChromeBrowserFieldTrials::RegisterSyntheticTrials().
     */
    @MainThread
    public static void createFirstRunTrial() {
        synchronized (LOCK) {
            // Don't create a new group if the user was already assigned a group. Can happen when
            // the user dismisses FRE without finishing the flow and cold starts chrome again.
            if (SharedPreferencesManager.getInstance().readString(
                        ChromePreferenceKeys.FIRST_RUN_FIELD_TRIAL_GROUP, null)
                    != null) {
                return;
            }
        }

        // Tweak these values for different builds to create the percentage of enabled population.
        // For A/B testing enabled and disabled group should have the same percentages.
        int enabledPercent = 0;
        int disabledPercent = 0;
        switch (VersionConstants.CHANNEL) {
            case Channel.DEFAULT:
            case Channel.CANARY:
            case Channel.DEV:
                enabledPercent = 50;
                disabledPercent = 50;
                break;
            case Channel.BETA:
            case Channel.STABLE:
        }
        assert enabledPercent + disabledPercent <= 100;

        int randomBucket = new Random().nextInt(100);
        String group = DEFAULT_GROUP;
        if (randomBucket < enabledPercent) {
            group = ENABLED_GROUP;
        } else if (randomBucket < enabledPercent + disabledPercent) {
            group = DISABLED_GROUP;
        }

        synchronized (LOCK) {
            SharedPreferencesManager.getInstance().writeString(
                    ChromePreferenceKeys.FIRST_RUN_FIELD_TRIAL_GROUP, group);
        }
    }
}