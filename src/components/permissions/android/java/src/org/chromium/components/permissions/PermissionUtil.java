// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.permissions;

import android.os.Build;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.content_settings.ContentSettingsType;

import java.util.Arrays;

/**
 * A utility class for permissions.
 */
public class PermissionUtil {
    /** The permissions associated with requesting location pre-Android S. */
    private static final String[] LOCATION_PERMISSIONS_PRE_S = {
            android.Manifest.permission.ACCESS_FINE_LOCATION,
            android.Manifest.permission.ACCESS_COARSE_LOCATION};
    /** The required Android permissions associated with requesting location post-Android S. */
    private static final String[] LOCATION_REQUIRED_PERMISSIONS_POST_S = {
            android.Manifest.permission.ACCESS_COARSE_LOCATION};
    /** The optional Android permissions associated with requesting location post-Android S. */
    private static final String[] LOCATION_OPTIONAL_PERMISSIONS_POST_S = {
            android.Manifest.permission.ACCESS_FINE_LOCATION};

    /** The android permissions associated with requesting access to the camera. */
    private static final String[] CAMERA_PERMISSIONS = {android.Manifest.permission.CAMERA};
    /** The android permissions associated with requesting access to the microphone. */
    private static final String[] MICROPHONE_PERMISSIONS = {
            android.Manifest.permission.RECORD_AUDIO};
    /** Signifies there are no permissions associated. */
    private static final String[] EMPTY_PERMISSIONS = {};

    private PermissionUtil() {}

    /** Whether precise/approximate location support is enabled. */
    private static boolean isApproximateLocationSupportEnabled() {
        // Even for apps targeting SDK version 30-, the user can downgrade location precision
        // in app settings if the device is running Android S. In addition, for apps targeting SDK
        // version 31+, users will be able to choose the precision in the permission dialog for apps
        // targeting SDK version 31. Therefore enable support based on the current device's
        // software's SDK version as opposed to Chrome's targetSdkVersion. See:
        // https://developer.android.com/about/versions/12/approximate-location
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.S
                && PermissionsAndroidFeatureList.isEnabled(
                        PermissionsAndroidFeatureList
                                .ANDROID_APPROXIMATE_LOCATION_PERMISSION_SUPPORT);
    }

    /**
     * Returns required Android permission strings for a given {@link ContentSettingsType}.  If
     * there is no permissions associated with the content setting, then an empty array is returned.
     *
     * @param contentSettingType The content setting to get the Android permissions for.
     * @return The required Android permissions for the given content setting. Permission sets
     *         returned for different content setting types are disjunct.
     */
    @CalledByNative
    public static String[] getRequiredAndroidPermissionsForContentSetting(int contentSettingType) {
        switch (contentSettingType) {
            case ContentSettingsType.GEOLOCATION:
                if (isApproximateLocationSupportEnabled()) {
                    return Arrays.copyOf(LOCATION_REQUIRED_PERMISSIONS_POST_S,
                            LOCATION_REQUIRED_PERMISSIONS_POST_S.length);
                }
                return Arrays.copyOf(LOCATION_PERMISSIONS_PRE_S, LOCATION_PERMISSIONS_PRE_S.length);
            case ContentSettingsType.MEDIASTREAM_MIC:
                return Arrays.copyOf(MICROPHONE_PERMISSIONS, MICROPHONE_PERMISSIONS.length);
            case ContentSettingsType.MEDIASTREAM_CAMERA:
            case ContentSettingsType.AR:
                return Arrays.copyOf(CAMERA_PERMISSIONS, CAMERA_PERMISSIONS.length);
            default:
                return EMPTY_PERMISSIONS;
        }
    }

    /**
     * Returns optional Android permission strings for a given {@link ContentSettingsType}.  If
     * there is no permissions associated with the content setting, or all of them are required,
     * then an empty array is returned.
     *
     * @param contentSettingType The content setting to get the Android permissions for.
     * @return The optional Android permissions for the given content setting. Permission sets
     *         returned for different content setting types are disjunct.
     */
    @CalledByNative
    public static String[] getOptionalAndroidPermissionsForContentSetting(int contentSettingType) {
        switch (contentSettingType) {
            case ContentSettingsType.GEOLOCATION:
                if (isApproximateLocationSupportEnabled()) {
                    return Arrays.copyOf(LOCATION_OPTIONAL_PERMISSIONS_POST_S,
                            LOCATION_OPTIONAL_PERMISSIONS_POST_S.length);
                }
                return EMPTY_PERMISSIONS;
            default:
                return EMPTY_PERMISSIONS;
        }
    }
}
