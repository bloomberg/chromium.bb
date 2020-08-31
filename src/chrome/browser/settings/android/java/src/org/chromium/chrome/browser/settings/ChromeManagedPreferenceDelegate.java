// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import androidx.preference.Preference;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;

/**
 * A ManagedPreferenceDelegate with Chrome-specific default behavior.
 */
public interface ChromeManagedPreferenceDelegate extends ManagedPreferenceDelegate {
    @Override
    default boolean isPreferenceControlledByCustodian(Preference preference) {
        return false;
    }

    @Override
    default boolean doesProfileHaveMultipleCustodians() {
        return !PrefServiceBridge.getInstance()
                        .getString(Pref.SUPERVISED_USER_SECOND_CUSTODIAN_NAME)
                        .isEmpty();
    }
}
