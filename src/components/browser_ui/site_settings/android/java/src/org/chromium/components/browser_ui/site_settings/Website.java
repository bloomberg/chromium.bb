// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import androidx.annotation.Nullable;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.StorageInfoClearedCallback;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

/**
 * Website is a class for storing information about a website and its associated permissions.
 */
public class Website implements Serializable {
    static final int INVALID_CAMERA_OR_MICROPHONE_ACCESS = 0;
    static final int CAMERA_ACCESS_ALLOWED = 1;
    static final int MICROPHONE_AND_CAMERA_ACCESS_ALLOWED = 2;
    static final int MICROPHONE_ACCESS_ALLOWED = 3;
    static final int CAMERA_ACCESS_DENIED = 4;
    static final int MICROPHONE_AND_CAMERA_ACCESS_DENIED = 5;
    static final int MICROPHONE_ACCESS_DENIED = 6;

    private final WebsiteAddress mOrigin;
    private final WebsiteAddress mEmbedder;

    /**
     * Indexed by ContentSettingException.Type.
     */
    private ContentSettingException mContentSettingException[];
    /**
     * Indexed by PermissionInfo.Type.
     */
    private PermissionInfo[] mPermissionInfo;

    private LocalStorageInfo mLocalStorageInfo;
    private final List<StorageInfo> mStorageInfo = new ArrayList<StorageInfo>();
    private int mStorageInfoCallbacksLeft;

    // The collection of chooser-based permissions (e.g. USB device access) granted to this site.
    // Each entry declares its own ContentSettingsType and so depending on how this object was
    // built this list could contain multiple types of objects.
    private final List<ChosenObjectInfo> mObjectInfo = new ArrayList<ChosenObjectInfo>();

    public Website(WebsiteAddress origin, WebsiteAddress embedder) {
        mOrigin = origin;
        mEmbedder = embedder;
        mPermissionInfo = new PermissionInfo[PermissionInfo.Type.NUM_ENTRIES];
        mContentSettingException =
                new ContentSettingException[ContentSettingException.Type.NUM_ENTRIES];
    }

    public WebsiteAddress getAddress() {
        return mOrigin;
    }

    public WebsiteAddress getEmbedder() {
        return mEmbedder;
    }

    public String getTitle() {
        return getMainAddress().getTitle();
    }

    public boolean representsThirdPartiesOnSite() {
        return mOrigin.getTitle().equals(SITE_WILDCARD) && mEmbedder != null
                && !mEmbedder.getTitle().equals(SITE_WILDCARD);
    }

    private WebsiteAddress getMainAddress() {
        if (representsThirdPartiesOnSite()) {
            return mEmbedder;
        }
        return mOrigin;
    }

    /**
     * Returns whichever WebsiteAddress is used to provide additional information. This will
     * either return null (representing the wildcard) if it a 3P exception, or the mEmbedder
     * (which may not be null, such as in the "a.com, b.com" origin combination).
     */
    private WebsiteAddress getAdditionalInformationAddress() {
        if (representsThirdPartiesOnSite()) return null;
        return mEmbedder;
    }

    /**
     * A comparison function for sorting by address (first by origin and then
     * by embedder).
     */
    public int compareByAddressTo(Website to) {
        if (this == to) return 0;

        // We want Website instances that represent third parties to be ordered beside other
        // Website instances with the same "main site".
        int originComparison = getMainAddress().compareTo(to.getMainAddress());

        if (originComparison == 0) {
            if (getAdditionalInformationAddress() == null) {
                return to.getAdditionalInformationAddress() == null ? 0 : -1;
            } else if (to.getAdditionalInformationAddress() == null) {
                return 1;
            }
            return getAdditionalInformationAddress().compareTo(
                    to.getAdditionalInformationAddress());
        }
        return originComparison;
    }

    /**
     * A comparison function for sorting by storage (most used first).
     * @return which site uses more storage.
     */
    public int compareByStorageTo(Website to) {
        if (this == to) return 0;
        return MathUtils.compareLongs(to.getTotalUsage(), getTotalUsage());
    }

    /**
     * @return PermissionInfo with permission details of specified type
     *         (Camera, Clipboard, etc.).
     */
    public PermissionInfo getPermissionInfo(@PermissionInfo.Type int type) {
        return mPermissionInfo[type];
    }

    /**
     * Set PermissionInfo for permission details of specified type
     * (Camera, Clipboard, etc.).
     */
    public void setPermissionInfo(PermissionInfo info) {
        mPermissionInfo[info.getType()] = info;
    }

    /**
     * @return permission value for permission of specified type.
     *         (Camera, Clipboard, etc.).
     */
    public @ContentSettingValues @Nullable Integer getPermission(
            BrowserContextHandle browserContextHandle, @PermissionInfo.Type int type) {
        return getPermissionInfo(type) != null
                ? getPermissionInfo(type).getContentSetting(browserContextHandle)
                : null;
    }

    /**
     * Set permission value for permission of specified type
     * (Camera, Clipboard, etc.).
     */
    public void setPermission(BrowserContextHandle browserContextHandle,
            @PermissionInfo.Type int type, @ContentSettingValues int value) {
        if (getPermissionInfo(type) != null) {
            getPermissionInfo(type).setContentSetting(browserContextHandle, value);
        }
    }

    /**
     * Returns the exception info for this Website for specified type.
     */
    public ContentSettingException getContentSettingException(
            @ContentSettingException.Type int type) {
        return mContentSettingException[type];
    }

    /**
     * Sets the exception info for this Website for specified type.
     */
    public void setContentSettingException(
            @ContentSettingException.Type int type, ContentSettingException exception) {
        mContentSettingException[type] = exception;
    }

    /**
     * Returns what ContentSettingException governs the setting of specified type.
     */
    public @ContentSettingValues @Nullable Integer getContentSettingPermission(
            @ContentSettingException.Type int type) {
        return mContentSettingException[type] != null
                ? mContentSettingException[type].getContentSetting()
                : null;
    }

    /**
     * Sets the permission.
     */
    public void setContentSettingPermission(BrowserContextHandle browserContextHandle,
            @ContentSettingException.Type int type, @ContentSettingValues int value) {
        if (type == ContentSettingException.Type.ADS) {
            // It is possible to set the permission without having an existing exception,
            // because we can show the BLOCK state even when this permission is set to the
            // default. In that case, just set an exception now to BLOCK to enable changing the
            // permission.
            if (mContentSettingException[type] == null) {
                mContentSettingException[type] =
                        new ContentSettingException(ContentSettingsType.ADS,
                                getAddress().getOrigin(), ContentSettingValues.BLOCK, "");
            }
        } else if (type == ContentSettingException.Type.JAVASCRIPT) {
            // It is possible to set the permission without having an existing exception,
            // because we show the javascript permission in Site Settings if javascript
            // is blocked by default.
            if (mContentSettingException[type] == null) {
                mContentSettingException[type] = new ContentSettingException(
                        ContentSettingsType.JAVASCRIPT, getAddress().getHost(), value, "");
            }
            // It's possible for either action to be emitted. This code path is hit
            // regardless of whether there was an existing permission or not.
            if (value == ContentSettingValues.BLOCK) {
                RecordUserAction.record("JavascriptContentSetting.EnableBy.SiteSettings");
            } else {
                RecordUserAction.record("JavascriptContentSetting.DisableBy.SiteSettings");
            }
        } else if (type == ContentSettingException.Type.SOUND) {
            // It is possible to set the permission without having an existing exception,
            // because we always show the sound permission in Site Settings.
            if (mContentSettingException[type] == null) {
                mContentSettingException[type] = new ContentSettingException(
                        ContentSettingsType.SOUND, getAddress().getHost(), value, "");
            }
            if (value == ContentSettingValues.BLOCK) {
                RecordUserAction.record("SoundContentSetting.MuteBy.SiteSettings");
            } else {
                RecordUserAction.record("SoundContentSetting.UnmuteBy.SiteSettings");
            }
        }
        // We want to call setContentSetting even after explicitly setting
        // mContentSettingException above because this will trigger the actual change
        // on the PrefServiceBridge.
        if (mContentSettingException[type] != null) {
            mContentSettingException[type].setContentSetting(browserContextHandle, value);
        }
    }

    public void setLocalStorageInfo(LocalStorageInfo info) {
        mLocalStorageInfo = info;
    }

    public LocalStorageInfo getLocalStorageInfo() {
        return mLocalStorageInfo;
    }

    public void addStorageInfo(StorageInfo info) {
        mStorageInfo.add(info);
    }

    public List<StorageInfo> getStorageInfo() {
        return new ArrayList<StorageInfo>(mStorageInfo);
    }

    public void clearAllStoredData(
            BrowserContextHandle browserContextHandle, final StoredDataClearedCallback callback) {
        // Wait for callbacks from each mStorageInfo and another callback from
        // mLocalStorageInfo.
        mStorageInfoCallbacksLeft = mStorageInfo.size() + 1;
        StorageInfoClearedCallback clearedCallback = () -> {
            if (--mStorageInfoCallbacksLeft == 0) callback.onStoredDataCleared();
        };
        if (mLocalStorageInfo != null) {
            mLocalStorageInfo.clear(browserContextHandle, clearedCallback);
            mLocalStorageInfo = null;
        } else {
            clearedCallback.onStorageInfoCleared();
        }
        for (StorageInfo info : mStorageInfo) info.clear(browserContextHandle, clearedCallback);
        mStorageInfo.clear();
    }

    /**
     * An interface to implement to get a callback when storage info has been cleared.
     */
    public interface StoredDataClearedCallback {
        public void onStoredDataCleared();
    }

    public long getTotalUsage() {
        long usage = 0;
        if (mLocalStorageInfo != null) usage += mLocalStorageInfo.getSize();
        for (StorageInfo info : mStorageInfo) usage += info.getSize();
        return usage;
    }

    /**
     * Add information about an object the user has granted permission for this site to access.
     */
    public void addChosenObjectInfo(ChosenObjectInfo info) {
        mObjectInfo.add(info);
    }

    /**
     * Returns the set of objects this website has been granted permission to access.
     */
    public List<ChosenObjectInfo> getChosenObjectInfo() {
        return new ArrayList<ChosenObjectInfo>(mObjectInfo);
    }
}
