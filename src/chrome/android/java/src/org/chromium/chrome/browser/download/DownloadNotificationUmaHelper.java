// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static android.app.DownloadManager.ACTION_NOTIFICATION_CLICKED;

import static org.chromium.chrome.browser.download.DownloadNotificationService2.ACTION_DOWNLOAD_CANCEL;
import static org.chromium.chrome.browser.download.DownloadNotificationService2.ACTION_DOWNLOAD_OPEN;
import static org.chromium.chrome.browser.download.DownloadNotificationService2.ACTION_DOWNLOAD_PAUSE;
import static org.chromium.chrome.browser.download.DownloadNotificationService2.ACTION_DOWNLOAD_RESUME;

import android.support.annotation.IntDef;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;

/**
 * Helper to track necessary stats in UMA related to downloads notifications.
 */
public final class DownloadNotificationUmaHelper {
    // The state of a download or offline page request at user-initiated cancel.
    // Keep in sync with enum OfflineItemsStateAtCancel in enums.xml.
    @IntDef({StateAtCancel.DOWNLOADING, StateAtCancel.PAUSED, StateAtCancel.PENDING_NETWORK,
            StateAtCancel.PENDING_ANOTHER_DOWNLOAD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StateAtCancel {
        int DOWNLOADING = 0;
        int PAUSED = 1;
        int PENDING_NETWORK = 2;
        int PENDING_ANOTHER_DOWNLOAD = 3;

        int NUM_ENTRIES = 4;
    }

    // NOTE: Keep these lists/classes in sync with DownloadNotification[...] in enums.xml.
    @IntDef({ForegroundLifecycle.START, ForegroundLifecycle.UPDATE, ForegroundLifecycle.STOP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ForegroundLifecycle {
        int START = 0; // Initial startForeground.
        int UPDATE = 1; // Switching pinned notification.
        int STOP = 2; // Calling stopForeground.
        int NUM_ENTRIES = 3;
    }

    private static List<String> sInteractions = Arrays.asList(
            ACTION_NOTIFICATION_CLICKED, // Opening a download where LegacyHelpers.isLegacyDownload.
            ACTION_DOWNLOAD_OPEN, // Opening a download that is not a legacy download.
            ACTION_DOWNLOAD_CANCEL, ACTION_DOWNLOAD_PAUSE, ACTION_DOWNLOAD_RESUME);

    @IntDef({LaunchType.LAUNCH, LaunchType.RELAUNCH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LaunchType {
        int LAUNCH = 0; // "Denominator" for expected launched notifications.
        int RELAUNCH = 1;
        int NUM_ENTRIES = 2;
    }

    @IntDef({ServiceStopped.STOPPED, ServiceStopped.DESTROYED, ServiceStopped.TASK_REMOVED,
            ServiceStopped.LOW_MEMORY, ServiceStopped.START_STICKY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ServiceStopped {
        int STOPPED = 0; // Expected, intentional stops, serves as a "denominator".
        int DESTROYED = 1;
        int TASK_REMOVED = 2;
        int LOW_MEMORY = 3;
        int START_STICKY = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * Records an instance where a user interacts with a notification (clicks on, pauses, etc).
     * @param action Notification interaction that was taken (ie. pause, resume).
     */
    static void recordNotificationInteractionHistogram(String action) {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        int actionType = sInteractions.indexOf(action);
        if (actionType == -1) return;
        RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.NotificationInteraction",
                actionType, sInteractions.size());
    }

    /**
     * Records an instance where the foreground stops, using expected stops as the denominator to
     * understand the frequency of unexpected stops (low memory, task removed, etc).
     * @param stopType Type of the foreground stop that is being recorded ({@link ServiceStopped}).
     */
    static void recordServiceStoppedHistogram(
            @ServiceStopped int stopType, boolean withForeground) {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        if (withForeground) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DownloadManager.ServiceStopped.DownloadForeground", stopType,
                    ServiceStopped.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DownloadManager.ServiceStopped.DownloadNotification", stopType,
                    ServiceStopped.NUM_ENTRIES);
        }
    }

    /**
     * Records an instance where the foreground undergoes a lifecycle change (when the foreground
     * starts, changes pinned notification, or stops).
     * @param lifecycleStep The lifecycle step that is being recorded ({@link ForegroundLifecycle}).
     */
    static void recordForegroundServiceLifecycleHistogram(@ForegroundLifecycle int lifecycleStep) {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.ForegroundServiceLifecycle", lifecycleStep,
                ForegroundLifecycle.NUM_ENTRIES);
    }

    /**
     * Record the number of existing notifications when a new notification is being launched (more
     * specifically the number of existing shared preference entries when a new shared preference
     * entry is being recorded).
     * @param count The number of existing notifications.
     * @param withForeground Whether this is with foreground enabled or not.
     */
    static void recordExistingNotificationsCountHistogram(int count, boolean withForeground) {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        if (withForeground) {
            RecordHistogram.recordCountHistogram(
                    "Android.DownloadManager.NotificationsCount.ForegroundEnabled", count);
        } else {
            RecordHistogram.recordCountHistogram(
                    "Android.DownloadManager.NotificationsCount.ForegroundDisabled", count);
        }
    }

    /**
     * Record an instance when a notification is being launched for the first time or relaunched due
     * to the need to dissociate the notification from the foreground (only on API < 24).
     * @param launchType Whether it is a launch or a relaunch ({@link LaunchType}).
     */
    static void recordNotificationFlickerCountHistogram(@LaunchType int launchType) {
        if (!LibraryLoader.getInstance().isInitialized()) return;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.NotificationLaunch", launchType, LaunchType.NUM_ENTRIES);
    }

    /**
     * Records the state of a request at user-initiated cancel.
     * @param isDownload True if the request is a download, false if it is an offline page.
     * @param state State of a request when cancelled (e.g. downloading, paused).
     */
    static void recordStateAtCancelHistogram(boolean isDownload, @StateAtCancel int state) {
        if (state == -1) return;
        if (!LibraryLoader.getInstance().isInitialized()) return;
        if (isDownload) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.OfflineItems.StateAtCancel.Downloads", state,
                    StateAtCancel.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.OfflineItems.StateAtCancel.OfflinePages", state,
                    StateAtCancel.NUM_ENTRIES);
        }
    }
}
