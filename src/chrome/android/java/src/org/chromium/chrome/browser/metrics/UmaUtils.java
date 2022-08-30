// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.app.ActivityManager;
import android.app.usage.UsageStatsManager;
import android.content.Context;
import android.os.Build;
import android.os.SystemClock;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.base.metrics.RecordHistogram;

/**
 * Utilities to support startup metrics - Android version.
 */
@JNINamespace("chrome::android")
public class UmaUtils {
    /** Observer for this class. */
    public interface Observer {
        /**
         * Called when hasComeToForeground() changes from false to true.
         */
        void onHasComeToForeground();
    }

    private static ObserverList<Observer> sObservers;

    /** Adds an observer. */
    public static boolean addObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) sObservers = new ObserverList<>();
        return sObservers.addObserver(observer);
    }

    /** Removes an observer. */
    public static boolean removeObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        if (sObservers == null) return false;
        return sObservers.removeObserver(observer);
    }

    // All these values originate from SystemClock.uptimeMillis().
    private static long sApplicationStartTimeMs;
    private static long sForegroundStartTimeMs;
    private static long sBackgroundTimeMs;

    private static boolean sSkipRecordingNextForegroundStartTimeForTesting;

    // Will short-circuit out of the next recordForegroundStartTime() call.
    public static void skipRecordingNextForegroundStartTimeForTesting() {
        sSkipRecordingNextForegroundStartTimeForTesting = true;
    }

    /**
     * App standby bucket status, used for UMA reporting. Enum values correspond to the return
     * values of {@link UsageStatsManager#getAppStandbyBucket}.
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     */
    @IntDef({StandbyBucketStatus.ACTIVE, StandbyBucketStatus.WORKING_SET,
            StandbyBucketStatus.FREQUENT, StandbyBucketStatus.RARE, StandbyBucketStatus.RESTRICTED,
            StandbyBucketStatus.UNSUPPORTED, StandbyBucketStatus.COUNT})
    private @interface StandbyBucketStatus {
        int ACTIVE = 0;
        int WORKING_SET = 1;
        int FREQUENT = 2;
        int RARE = 3;
        int RESTRICTED = 4;
        int UNSUPPORTED = 5;
        int COUNT = 6;
    }

    /**
     * Record the time in the application lifecycle at which Chrome code first runs
     * (Application.attachBaseContext()).
     */
    public static void recordMainEntryPointTime() {
        // We can't simply pass this down through a JNI call, since the JNI for chrome
        // isn't initialized until we start the native content browser component, and we
        // then need the start time in the C++ side before we return to Java. As such we
        // save it in a static that the C++ can fetch once it has initialized the JNI.
        sApplicationStartTimeMs = SystemClock.uptimeMillis();
    }

    /**
     * Record the time at which Chrome was brought to foreground.
     */
    public static void recordForegroundStartTime() {
        if (sSkipRecordingNextForegroundStartTimeForTesting) {
            sSkipRecordingNextForegroundStartTimeForTesting = false;
            return;
        }

        // Since this can be called from multiple places (e.g. ChromeActivitySessionTracker
        // and FirstRunActivity), only set the time if it hasn't been set previously or if
        // Chrome has been sent to background since the last foreground time.
        if (sForegroundStartTimeMs == 0 || sForegroundStartTimeMs < sBackgroundTimeMs) {
            if (sObservers != null && sForegroundStartTimeMs == 0) {
                for (Observer observer : sObservers) {
                    observer.onHasComeToForeground();
                }
            }

            sForegroundStartTimeMs = SystemClock.uptimeMillis();
        }
    }

    /**
     * Record the time at which Chrome was sent to background.
     */
    public static void recordBackgroundTime() {
        sBackgroundTimeMs = SystemClock.uptimeMillis();
    }

    /**
     * Determines if Chrome was brought to foreground.
     */
    public static boolean hasComeToForeground() {
        return sForegroundStartTimeMs != 0;
    }

    /**
     * Determines if Chrome was brought to background.
     */
    public static boolean hasComeToBackground() {
        return sBackgroundTimeMs != 0;
    }

    /**
     * Determines if this client is eligible to send metrics and crashes based on sampling. If it
     * is, and there was user consent, then metrics and crashes would be reported
     */
    public static boolean isClientInMetricsReportingSample() {
        return UmaUtilsJni.get().isClientInMetricsReportingSample();
    }

    /**
     * Records various levels of background restrictions imposed by android on chrome.
     */
    public static void recordBackgroundRestrictions() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;
        Context context = ContextUtils.getApplicationContext();
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        boolean isBackgroundRestricted = activityManager.isBackgroundRestricted();
        RecordHistogram.recordBooleanHistogram(
                "Android.BackgroundRestrictions.IsBackgroundRestricted", isBackgroundRestricted);

        int standbyBucketUma = getStandbyBucket(context);
        RecordHistogram.recordEnumeratedHistogram("Android.BackgroundRestrictions.StandbyBucket",
                standbyBucketUma, StandbyBucketStatus.COUNT);

        String histogramNameSplitByUserRestriction = isBackgroundRestricted
                ? "Android.BackgroundRestrictions.StandbyBucket.WithUserRestriction"
                : "Android.BackgroundRestrictions.StandbyBucket.WithoutUserRestriction";
        RecordHistogram.recordEnumeratedHistogram(
                histogramNameSplitByUserRestriction, standbyBucketUma, StandbyBucketStatus.COUNT);
    }

    /** Record minidump uploading time split by background restriction status. */
    public static void recordMinidumpUploadingTime(long taskDurationMs) {
        RecordHistogram.recordCustomTimesHistogram("Stability.Android.MinidumpUploadingTime",
                taskDurationMs, 1, DateUtils.DAY_IN_MILLIS, 50);

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;
        RecordHistogram.recordCustomTimesHistogram(
                "Stability.Android.MinidumpUploadingTime." + getHistogramPatternForStandbyStatus(),
                taskDurationMs, 1, DateUtils.DAY_IN_MILLIS, 50);
    }

    private static String getHistogramPatternForStandbyStatus() {
        int standbyBucket = getStandbyBucket(ContextUtils.getApplicationContext());
        switch (standbyBucket) {
            case StandbyBucketStatus.ACTIVE:
                return "Active";
            case StandbyBucketStatus.WORKING_SET:
                return "WorkingSet";
            case StandbyBucketStatus.FREQUENT:
                return "Frequent";
            case StandbyBucketStatus.RARE:
                return "Rare";
            case StandbyBucketStatus.RESTRICTED:
                return "Restricted";
            case StandbyBucketStatus.UNSUPPORTED:
                return "Unsupported";
            default:
                assert false : "Unexpected standby bucket " + standbyBucket;
                return "Unknown";
        }
    }

    @StandbyBucketStatus
    private static int getStandbyBucket(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return StandbyBucketStatus.UNSUPPORTED;

        UsageStatsManager usageStatsManager =
                (UsageStatsManager) context.getSystemService(Context.USAGE_STATS_SERVICE);
        int standbyBucket = usageStatsManager.getAppStandbyBucket();
        int standbyBucketUma = StandbyBucketStatus.UNSUPPORTED;
        switch (standbyBucket) {
            case UsageStatsManager.STANDBY_BUCKET_ACTIVE:
                standbyBucketUma = StandbyBucketStatus.ACTIVE;
                break;
            case UsageStatsManager.STANDBY_BUCKET_WORKING_SET:
                standbyBucketUma = StandbyBucketStatus.WORKING_SET;
                break;
            case UsageStatsManager.STANDBY_BUCKET_FREQUENT:
                standbyBucketUma = StandbyBucketStatus.FREQUENT;
                break;
            case UsageStatsManager.STANDBY_BUCKET_RARE:
                standbyBucketUma = StandbyBucketStatus.RARE;
                break;
            case UsageStatsManager.STANDBY_BUCKET_RESTRICTED:
                standbyBucketUma = StandbyBucketStatus.RESTRICTED;
                break;
            default:
                assert false : "Unexpected standby bucket " + standbyBucket;
        }
        return standbyBucketUma;
    }

    /**
     * Sets whether metrics reporting was opt-in or not. If it was opt-in, then the enable checkbox
     * on first-run was default unchecked. If it was opt-out, then the checkbox was default checked.
     * This should only be set once, and only during first-run.
     */
    public static void recordMetricsReportingDefaultOptIn(boolean optIn) {
        UmaUtilsJni.get().recordMetricsReportingDefaultOptIn(optIn);
    }

    @CalledByNative
    public static long getApplicationStartTime() {
        return sApplicationStartTimeMs;
    }

    @CalledByNative
    public static long getProcessStartTime() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            return 0;
        }
        return ApiHelperForN.getStartUptimeMillis();
    }

    @NativeMethods
    interface Natives {
        boolean isClientInMetricsReportingSample();
        void recordMetricsReportingDefaultOptIn(boolean optIn);
    }
}
