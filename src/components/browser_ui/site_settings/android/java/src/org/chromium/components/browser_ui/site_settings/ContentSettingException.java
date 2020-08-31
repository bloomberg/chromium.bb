// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

import java.io.Serializable;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Exception information for a given origin.
 */
public class ContentSettingException implements Serializable {
    @IntDef({Type.ADS, Type.AUTOMATIC_DOWNLOADS, Type.BACKGROUND_SYNC, Type.COOKIE, Type.JAVASCRIPT,
            Type.POPUP, Type.SOUND, Type.BLUETOOTH_SCANNING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        // Values used to address array index - should be enumerated from 0 and can't have gaps.
        // All updates here must also be reflected in {@link #getContentSettingsType(int)
        // getContentSettingsType} and {@link SingleWebsiteSettings.PERMISSION_PREFERENCE_KEYS}.
        int ADS = 0;
        int BACKGROUND_SYNC = 1;
        int COOKIE = 2;
        int JAVASCRIPT = 3;
        int POPUP = 4;
        int SOUND = 5;
        int AUTOMATIC_DOWNLOADS = 6;
        int BLUETOOTH_SCANNING = 7;
        /**
         * Number of handled exceptions used for calculating array sizes.
         */
        int NUM_ENTRIES = 8;
    }

    private final int mContentSettingType;
    private final String mPrimaryPattern;
    private final String mSecondaryPattern;
    private final @ContentSettingValues @Nullable Integer mContentSetting;
    private final String mSource;

    /**
     * Construct a ContentSettingException.
     * @param type The content setting type this exception covers.
     * @param primaryPattern The primary host/domain pattern this exception covers.
     * @param secondaryPattern The secondary host/domain pattern this exception covers.
     * @param setting The setting for this exception, e.g. ALLOW or BLOCK.
     * @param source The source for this exception, e.g. "policy".
     */
    public ContentSettingException(int type, String primaryPattern, String secondaryPattern,
            @ContentSettingValues @Nullable Integer setting, String source) {
        mContentSettingType = type;
        mPrimaryPattern = primaryPattern;
        mSecondaryPattern = secondaryPattern;
        mContentSetting = setting;
        mSource = source;
    }

    /**
     * Construct a ContentSettingException.
     * Same as above but defaults secondaryPattern to wildcard.
     */
    public ContentSettingException(int type, String primaryPattern,
            @ContentSettingValues @Nullable Integer setting, String source) {
        this(type, primaryPattern, SITE_WILDCARD, setting, source);
    }

    public String getPrimaryPattern() {
        return mPrimaryPattern;
    }

    public String getSecondaryPattern() {
        return mSecondaryPattern;
    }

    public String getSource() {
        return mSource;
    }

    public @ContentSettingValues @Nullable Integer getContentSetting() {
        return mContentSetting;
    }

    public int getContentSettingType() {
        return mContentSettingType;
    }

    /**
     * Sets the content setting value for this exception.
     */
    public void setContentSetting(BrowserContextHandle browserContextHandle,
            @ContentSettingValues @Nullable Integer value) {
        WebsitePreferenceBridge.setContentSettingForPattern(browserContextHandle,
                mContentSettingType, mPrimaryPattern, mSecondaryPattern, value);
    }

    public static @ContentSettingsType int getContentSettingsType(@Type int type) {
        switch (type) {
            case Type.ADS:
                return ContentSettingsType.ADS;
            case Type.AUTOMATIC_DOWNLOADS:
                return ContentSettingsType.AUTOMATIC_DOWNLOADS;
            case Type.BACKGROUND_SYNC:
                return ContentSettingsType.BACKGROUND_SYNC;
            case Type.BLUETOOTH_SCANNING:
                return ContentSettingsType.BLUETOOTH_SCANNING;
            case Type.COOKIE:
                return ContentSettingsType.COOKIES;
            case Type.JAVASCRIPT:
                return ContentSettingsType.JAVASCRIPT;
            case Type.POPUP:
                return ContentSettingsType.POPUPS;
            case Type.SOUND:
                return ContentSettingsType.SOUND;
            default:
                assert false;
                return ContentSettingsType.DEFAULT;
        }
    }
}
