// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.NonNull;

import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.ContentFeatureList;

/**
 * Util class for site settings UI.
 */
public class SiteSettingsUtil {
    // Defining the order for content settings based on http://crbug.com/610358
    static final int[] SETTINGS_ORDER = {
            ContentSettingsType.COOKIES,
            ContentSettingsType.GEOLOCATION,
            ContentSettingsType.MEDIASTREAM_CAMERA,
            ContentSettingsType.MEDIASTREAM_MIC,
            ContentSettingsType.NOTIFICATIONS,
            ContentSettingsType.JAVASCRIPT,
            ContentSettingsType.POPUPS,
            ContentSettingsType.ADS,
            ContentSettingsType.BACKGROUND_SYNC,
            ContentSettingsType.AUTOMATIC_DOWNLOADS,
            ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER,
            ContentSettingsType.SOUND,
            ContentSettingsType.MIDI_SYSEX,
            ContentSettingsType.CLIPBOARD_READ_WRITE,
            ContentSettingsType.NFC,
            ContentSettingsType.BLUETOOTH_SCANNING,
            ContentSettingsType.VR,
            ContentSettingsType.AR,
            ContentSettingsType.IDLE_DETECTION,
            ContentSettingsType.SENSORS,
    };

    static final int[] CHOOSER_PERMISSIONS = {
            ContentSettingsType.USB_CHOOSER_DATA,
            // Bluetooth is only shown when WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND is enabled.
            ContentSettingsType.BLUETOOTH_CHOOSER_DATA,
    };

    /**
     * @param types A list of ContentSettingsTypes
     * @return The highest priority permission that is available in SiteSettings. Returns DEFAULT
     *         when called with empty list or only with entries not represented in this UI.
     */
    public static @ContentSettingsType int getHighestPriorityPermission(
            @ContentSettingsType @NonNull int[] types) {
        for (@ContentSettingsType int setting : SETTINGS_ORDER) {
            for (@ContentSettingsType int type : types) {
                if (setting == type) {
                    return type;
                }
            }
        }
        for (@ContentSettingsType int setting : CHOOSER_PERMISSIONS) {
            for (@ContentSettingsType int type : types) {
                if (type == ContentSettingsType.BLUETOOTH_CHOOSER_DATA
                        && !ContentFeatureList.isEnabled(
                                ContentFeatureList.WEB_BLUETOOTH_NEW_PERMISSIONS_BACKEND)) {
                    continue;
                }
                if (setting == type) {
                    return type;
                }
            }
        }
        return ContentSettingsType.DEFAULT;
    }
}
