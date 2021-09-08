// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Gets and sets preferences associated with cloud management.
 */
@JNINamespace("policy::android")
public class CloudManagementSharedPreferences {
    /**
     * Sets the "Cloud management DM token" preference.
     *
     * @param dmToken The token provided by the DM server when browser registration succeeds.
     */
    @CalledByNative
    public static void saveDmToken(String dmToken) {
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, dmToken);
    }

    /**
     * Returns the value of the "Cloud management DM token" preference, which is non-empty
     * if browser registration succeeded.
     */
    @CalledByNative
    public static String readDmToken() {
        return SharedPreferencesManager.getInstance().readString(
                ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, "");
    }
}
