// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.location.LocationUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Utility class that interacts with native to retrieve and set website settings.
 */
public class WebsitePreferenceBridge {
    public static final String SITE_WILDCARD = "*";

    /**
     * Interface for an object that listens to storage info is cleared callback.
     */
    public interface StorageInfoClearedCallback {
        @CalledByNative("StorageInfoClearedCallback")
        public void onStorageInfoCleared();
    }

    /**
     * @return the list of all origins that have permissions in non-incognito mode.
     */
    @SuppressWarnings("unchecked")
    public List<PermissionInfo> getPermissionInfo(
            BrowserContextHandle browserContextHandle, @ContentSettingsType int type) {
        ArrayList<PermissionInfo> list = new ArrayList<PermissionInfo>();
        boolean managedOnly = false;
        // Camera, Location & Microphone can be managed by the custodian
        // of a supervised account or by enterprise policy.
        switch (type) {
            case ContentSettingsType.GEOLOCATION:
            case ContentSettingsType.MEDIASTREAM_CAMERA:
            case ContentSettingsType.MEDIASTREAM_MIC:
                managedOnly = !isContentSettingUserModifiable(browserContextHandle, type);
        }
        WebsitePreferenceBridgeJni.get().getOriginsForPermission(
                browserContextHandle, type, list, managedOnly);
        return list;
    }

    @CalledByNative
    private static void insertPermissionInfoIntoList(@ContentSettingsType int type,
            ArrayList<PermissionInfo> list, String origin, String embedder, boolean isEmbargoed) {
        if (type == ContentSettingsType.MEDIASTREAM_CAMERA
                || type == ContentSettingsType.MEDIASTREAM_MIC) {
            for (PermissionInfo info : list) {
                if (info.getOrigin().equals(origin) && info.getEmbedder().equals(embedder)) {
                    return;
                }
            }
        }
        list.add(new PermissionInfo(type, origin, embedder, isEmbargoed));
    }

    @CalledByNative
    private static void insertStorageInfoIntoList(
            ArrayList<StorageInfo> list, String host, int type, long size) {
        list.add(new StorageInfo(host, type, size));
    }

    @CalledByNative
    private static Object createStorageInfoList() {
        return new ArrayList<StorageInfo>();
    }

    @CalledByNative
    private static Object createLocalStorageInfoMap() {
        return new HashMap<String, LocalStorageInfo>();
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void insertLocalStorageInfoIntoMap(
            HashMap map, String origin, long size, boolean important) {
        ((HashMap<String, LocalStorageInfo>) map)
                .put(origin, new LocalStorageInfo(origin, size, important));
    }

    public List<ContentSettingException> getContentSettingsExceptions(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType) {
        List<ContentSettingException> exceptions = new ArrayList<>();
        WebsitePreferenceBridgeJni.get().getContentSettingsExceptions(
                browserContextHandle, contentSettingsType, exceptions);
        if (!isContentSettingManaged(browserContextHandle, contentSettingsType)) {
            return exceptions;
        }

        List<ContentSettingException> managedExceptions = new ArrayList<ContentSettingException>();
        for (ContentSettingException exception : exceptions) {
            if (exception.getSource().equals("policy")) {
                managedExceptions.add(exception);
            }
        }
        return managedExceptions;
    }

    public void fetchLocalStorageInfo(BrowserContextHandle browserContextHandle,
            Callback<HashMap> callback, boolean fetchImportant) {
        WebsitePreferenceBridgeJni.get().fetchLocalStorageInfo(
                browserContextHandle, callback, fetchImportant);
    }

    public void fetchStorageInfo(
            BrowserContextHandle browserContextHandle, Callback<ArrayList> callback) {
        WebsitePreferenceBridgeJni.get().fetchStorageInfo(browserContextHandle, callback);
    }

    /**
     * Returns the list of all chosen object permissions for the given ContentSettingsType.
     *
     * There will be one ChosenObjectInfo instance for each granted permission. That means that if
     * two origin/embedder pairs have permission for the same object there will be two
     * ChosenObjectInfo instances.
     */
    public List<ChosenObjectInfo> getChosenObjectInfo(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType) {
        ArrayList<ChosenObjectInfo> list = new ArrayList<ChosenObjectInfo>();
        WebsitePreferenceBridgeJni.get().getChosenObjects(
                browserContextHandle, contentSettingsType, list);
        return list;
    }

    /**
     * Inserts a ChosenObjectInfo into a list.
     */
    @CalledByNative
    private static void insertChosenObjectInfoIntoList(ArrayList<ChosenObjectInfo> list,
            @ContentSettingsType int contentSettingsType, String origin, String embedder,
            String name, String object, boolean isManaged) {
        list.add(new ChosenObjectInfo(
                contentSettingsType, origin, embedder, name, object, isManaged));
    }

    /**
     * Returns whether the DSE (Default Search Engine) controls the given permission the given
     * origin.
     */
    public static boolean isPermissionControlledByDSE(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType, String origin) {
        return WebsitePreferenceBridgeJni.get().isPermissionControlledByDSE(
                browserContextHandle, contentSettingsType, origin);
    }

    /**
     * Returns whether this origin is activated for ad blocking, and will have resources blocked
     * unless they are explicitly allowed via a permission.
     */
    public static boolean getAdBlockingActivated(
            BrowserContextHandle browserContextHandle, String origin) {
        return WebsitePreferenceBridgeJni.get().getAdBlockingActivated(
                browserContextHandle, origin);
    }

    @CalledByNative
    private static void addContentSettingExceptionToList(ArrayList<ContentSettingException> list,
            @ContentSettingsType int contentSettingsType, String primaryPattern,
            String secondaryPattern, int contentSetting, String source) {
        ContentSettingException exception = new ContentSettingException(
                contentSettingsType, primaryPattern, secondaryPattern, contentSetting, source);
        list.add(exception);
    }

    /**
     * Returns whether a particular content setting type is enabled.
     * @param contentSettingsType The content setting type to check.
     */
    public static boolean isContentSettingEnabled(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().isContentSettingEnabled(
                browserContextHandle, contentSettingsType);
    }

    /**
     * @return Whether a particular content setting type is managed by policy.
     * @param contentSettingsType The content setting type to check.
     */
    public static boolean isContentSettingManaged(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().isContentSettingManaged(
                browserContextHandle, contentSettingsType);
    }

    /**
     * @return Whether a particular content setting type is managed by custodian.
     * @param contentSettingsType The content setting type to check.
     */
    public static boolean isContentSettingManagedByCustodian(
            BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().isContentSettingManagedByCustodian(
                browserContextHandle, contentSettingsType);
    }

    /**
     * Sets a default value for content setting type.
     * @param contentSettingsType The content setting type to check.
     * @param enabled Whether the default value should be disabled or enabled.
     */
    public static void setContentSettingEnabled(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType, boolean enabled) {
        WebsitePreferenceBridgeJni.get().setContentSettingEnabled(
                browserContextHandle, contentSettingsType, enabled);
    }

    /**
     * Whether the setting type requires tri-state (Allowed/Ask/Blocked) setting.
     */
    public static boolean requiresTriStateContentSetting(
            @ContentSettingsType int contentSettingsType) {
        switch (contentSettingsType) {
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
                return true;
            default:
                return false;
        }
    }

    /**
     * Whether the setting type requires four-state
     * (Allow/BlockThirdPartyIncognito/BlockThirdParty/Block) setting.
     */
    public static boolean requiresFourStateContentSetting(
            @ContentSettingsType int contentSettingsType) {
        return contentSettingsType == ContentSettingsType.COOKIES;
    }

    /**
     * Sets the preferences on whether to enable/disable given setting.
     */
    public static void setCategoryEnabled(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType, boolean allow) {
        assert !requiresTriStateContentSetting(contentSettingsType);
        setContentSettingEnabled(browserContextHandle, contentSettingsType, allow);
    }

    public static boolean isCategoryEnabled(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType) {
        assert !requiresTriStateContentSetting(contentSettingsType);
        return isContentSettingEnabled(browserContextHandle, contentSettingsType);
    }

    /**
     * Gets the ContentSetting for a settings type. Should only be used for more
     * complex settings where a binary on/off value is not sufficient.
     * Otherwise, use isCategoryEnabled() above.
     * @param contentSettingsType The settings type to get setting for.
     * @return The ContentSetting for |contentSettingsType|.
     */
    public static int getContentSetting(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().getContentSetting(
                browserContextHandle, contentSettingsType);
    }

    /**
     * @param setting New ContentSetting to set for |contentSettingsType|.
     */
    public static void setContentSetting(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType, int setting) {
        WebsitePreferenceBridgeJni.get().setContentSetting(
                browserContextHandle, contentSettingsType, setting);
    }

    /**
     * Some Google-affiliated domains are not allowed to delete cookies for supervised accounts.
     *
     * @return Whether deleting cookies is disabled for |origin|.
     */
    public static boolean isCookieDeletionDisabled(
            BrowserContextHandle browserContextHandle, String origin) {
        return WebsitePreferenceBridgeJni.get().isCookieDeletionDisabled(
                browserContextHandle, origin);
    }

    /**
     * @return Whether geolocation information access is set to be shared with all sites, by policy.
     */
    public static boolean isLocationAllowedByPolicy(BrowserContextHandle browserContextHandle) {
        return WebsitePreferenceBridgeJni.get().getLocationAllowedByPolicy(browserContextHandle);
    }

    /**
     * @return Whether location is enabled system-wide and the Chrome location setting is enabled.
     */
    public static boolean areAllLocationSettingsEnabled(BrowserContextHandle browserContextHandle) {
        return isContentSettingEnabled(browserContextHandle, ContentSettingsType.GEOLOCATION)
                && LocationUtils.getInstance().isSystemLocationSettingEnabled();
    }

    /**
     * @return Whether the camera permission is editable by the user.
     */
    public static boolean isContentSettingUserModifiable(BrowserContextHandle browserContextHandle,
            @ContentSettingsType int contentSettingsType) {
        return WebsitePreferenceBridgeJni.get().isContentSettingUserModifiable(
                browserContextHandle, contentSettingsType);
    }

    public static void setContentSettingForPattern(BrowserContextHandle browserContextHandle,
            int contentSettingType, String primaryPattern, String secondaryPattern, int setting) {
        // Currently only Cookie Settings support a non-empty, non-wildcard secondaryPattern.
        // In addition, if a Cookie Setting uses secondaryPattern, the primaryPattern must be
        // the wildcard.
        if (contentSettingType != ContentSettingsType.COOKIES) {
            assert secondaryPattern.equals(SITE_WILDCARD) || secondaryPattern.isEmpty();
        } else if (!secondaryPattern.equals(SITE_WILDCARD) && !secondaryPattern.isEmpty()) {
            assert primaryPattern.equals(SITE_WILDCARD);
        }

        WebsitePreferenceBridgeJni.get().setContentSettingForPattern(browserContextHandle,
                contentSettingType, primaryPattern, secondaryPattern, setting);
    }

    @NativeMethods
    public interface Natives {
        boolean isNotificationEmbargoedForOrigin(
                BrowserContextHandle browserContextHandle, String origin);
        void reportNotificationRevokedForOrigin(
                BrowserContextHandle browserContextHandle, String origin, int newSettingValue);
        void clearBannerData(BrowserContextHandle browserContextHandle, String origin);
        void clearMediaLicenses(BrowserContextHandle browserContextHandle, String origin);
        void clearCookieData(BrowserContextHandle browserContextHandle, String path);
        void clearLocalStorageData(
                BrowserContextHandle browserContextHandle, String path, Object callback);
        void clearStorageData(BrowserContextHandle browserContextHandle, String origin, int type,
                Object callback);
        void getChosenObjects(BrowserContextHandle browserContextHandle,
                @ContentSettingsType int type, Object list);
        void resetNotificationsSettingsForTest(BrowserContextHandle browserContextHandle);
        void revokeObjectPermission(BrowserContextHandle browserContextHandle,
                @ContentSettingsType int type, String origin, String embedder, String object);
        boolean isContentSettingsPatternValid(String pattern);
        boolean urlMatchesContentSettingsPattern(String url, String pattern);
        void fetchStorageInfo(BrowserContextHandle browserContextHandle, Object callback);
        void fetchLocalStorageInfo(BrowserContextHandle browserContextHandle, Object callback,
                boolean includeImportant);
        void getOriginsForPermission(BrowserContextHandle browserContextHandle,
                @ContentSettingsType int contentSettingsType, Object list, boolean managedOnly);
        int getSettingForOrigin(BrowserContextHandle browserContextHandle,
                @ContentSettingsType int contentSettingsType, String origin, String embedder);
        void setSettingForOrigin(BrowserContextHandle browserContextHandle,
                @ContentSettingsType int contentSettingsType, String origin, String embedder,
                int value);
        boolean isPermissionControlledByDSE(BrowserContextHandle browserContextHandle,
                @ContentSettingsType int contentSettingsType, String origin);
        boolean getAdBlockingActivated(BrowserContextHandle browserContextHandle, String origin);
        boolean isContentSettingEnabled(
                BrowserContextHandle browserContextHandle, int contentSettingType);
        boolean isContentSettingManaged(
                BrowserContextHandle browserContextHandle, int contentSettingType);
        boolean isCookieDeletionDisabled(BrowserContextHandle browserContextHandle, String origin);
        void setContentSettingEnabled(
                BrowserContextHandle browserContextHandle, int contentSettingType, boolean allow);
        void getContentSettingsExceptions(BrowserContextHandle browserContextHandle,
                @ContentSettingsType int contentSettingsType, List<ContentSettingException> list);
        void setContentSettingForPattern(BrowserContextHandle browserContextHandle,
                int contentSettingType, String primaryPattern, String secondaryPattern,
                int setting);
        int getContentSetting(BrowserContextHandle browserContextHandle, int contentSettingType);
        void setContentSetting(
                BrowserContextHandle browserContextHandle, int contentSettingType, int setting);
        boolean isContentSettingUserModifiable(
                BrowserContextHandle browserContextHandle, int contentSettingType);
        boolean isContentSettingManagedByCustodian(
                BrowserContextHandle browserContextHandle, int contentSettingType);
        boolean getLocationAllowedByPolicy(BrowserContextHandle browserContextHandle);
    }
}
