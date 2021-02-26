// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import androidx.annotation.Nullable;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

import java.io.Serializable;

/**
 * Exception information for a given origin.
 */
public class ContentSettingException implements Serializable {
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
    public void setContentSetting(
            BrowserContextHandle browserContextHandle, @ContentSettingValues int value) {
        WebsitePreferenceBridge.setContentSettingForPattern(browserContextHandle,
                mContentSettingType, mPrimaryPattern, mSecondaryPattern, value);
    }
}
