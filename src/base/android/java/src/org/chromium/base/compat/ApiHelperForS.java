// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.annotation.TargetApi;
import android.app.PictureInPictureParams;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.pm.PackageManager;
import android.os.Process;
import android.view.textclassifier.TextLinks;

import androidx.annotation.NonNull;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.VerifiesOnS;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Utility class to use new APIs that were added in S (API level 31). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@VerifiesOnS
@TargetApi(BuildInfo.ANDROID_S_API_SDK_INT)
public final class ApiHelperForS {
    private static final String TAG = "ApiHelperForS";
    private static final int INVALID_STATUS = -1;

    private ApiHelperForS() {}

    /**
     * See {@link ClipDescription#isStyleText()}.
     */
    public static boolean isStyleText(ClipDescription clipDescription) {
        try {
            Method isStyledTextMethod = ClipDescription.class.getDeclaredMethod("isStyledText");
            return (boolean) isStyledTextMethod.invoke(clipDescription);
        } catch (InvocationTargetException | NoSuchMethodException | IllegalAccessException e) {
            Log.e(TAG, "Failed to invoke ClipDescription#isStyledText() ", e);
            return false;
        }
    }

    /**
     * See {@link ClipDescription#getConfidenceScore()}.
     */
    public static float getConfidenceScore(
            ClipDescription clipDescription, @NonNull String entityType) {
        try {
            Method getConfidenceScoreMethod =
                    ClipDescription.class.getDeclaredMethod("getConfidenceScore", String.class);
            return (float) getConfidenceScoreMethod.invoke(clipDescription, entityType);
        } catch (InvocationTargetException | NoSuchMethodException | IllegalAccessException
                | IllegalStateException e) {
            Log.e(TAG, "Failed to invoke ClipDescription#getConfidenceScore() ", e);
            return 0;
        }
    }

    /**
     * Return true if {@link ClipDescription#getClassificationStatus()} returns
     * ClipDescription.CLASSIFICATION_COMPLETE.
     */
    public static boolean isGetClassificationStatusIsComplete(ClipDescription clipDescription) {
        int status = getClassificationStatus(clipDescription);
        if (status == INVALID_STATUS) return false;
        try {
            Field ClassificationComplete =
                    ClipDescription.class.getField("CLASSIFICATION_COMPLETE");
            return status == ClassificationComplete.getInt(null);
        } catch (NoSuchFieldException | IllegalAccessException e) {
            Log.e(TAG, "Failed to get ClipDescription#CLASSIFICATION_COMPLETE ", e);
            return false;
        }
    }

    /**
     * See {@link ClipDescription#getClassificationStatus()}.
     */
    private static int getClassificationStatus(ClipDescription clipDescription) {
        try {
            Method getClassificationStatusMethod =
                    ClipDescription.class.getDeclaredMethod("getClassificationStatus");
            return (int) getClassificationStatusMethod.invoke(clipDescription);
        } catch (InvocationTargetException | NoSuchMethodException | IllegalAccessException
                | IllegalStateException e) {
            Log.e(TAG, "Failed to invoke ClipDescription#getClassificationStatus() ", e);
            return INVALID_STATUS;
        }
    }

    /**
     * See {@link ClipData.Item#getTextLinks()}.
     */
    public static TextLinks getTextLinks(ClipData.Item item) {
        try {
            Method getTextLinksMethod = ClipData.Item.class.getDeclaredMethod("getTextLinks");
            return (TextLinks) getTextLinksMethod.invoke(item);
        } catch (InvocationTargetException | NoSuchMethodException | IllegalAccessException e) {
            Log.e(TAG, "Failed to invoke ClipData.Item#getTextLinks() ", e);
            return null;
        }
    }

    public static boolean hasBluetoothConnectPermission() {
        // TODO(b/183501112): Replace the permission string with the actual Manfifest constant once
        // Chrome starts compiling against the S SDK.
        return ApiCompatibilityUtils.checkPermission(ContextUtils.getApplicationContext(),
                       "android.permission.BLUETOOTH_CONNECT", Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * See {@link android.app.PictureInPictureParams.Builder#setAutoEnterEnabled(boolean)}
     */
    public static void setAutoEnterEnabled(
            PictureInPictureParams.Builder builder, boolean enabled) {
        try {
            Method setAutoEnterEnabledMethod =
                    PictureInPictureParams.Builder.class.getDeclaredMethod(
                            "setAutoEnterEnabled", boolean.class);
            setAutoEnterEnabledMethod.invoke(builder, enabled);
        } catch (InvocationTargetException | NoSuchMethodException | IllegalAccessException e) {
            Log.e(TAG, "Failed to invoke PictureInPictureParams.Builder#setAutoEnterEnabled() ", e);
        }
    }
}
