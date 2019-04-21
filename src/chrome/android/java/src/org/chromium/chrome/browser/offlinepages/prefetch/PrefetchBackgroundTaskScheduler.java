// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import android.os.Bundle;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

/**
 * Handles scheduling background task for offline pages prefetching.
 */
@JNINamespace("offline_pages::prefetch")
public class PrefetchBackgroundTaskScheduler {
    public static final long DEFAULT_START_DELAY_SECONDS = 15 * 60;

    /**
     * Schedules the default 'NWake' task for the prefetching service. This task will normally be
     * scheduled on a good network type.
     * TODO(dewittj): Handle skipping work if the battery percentage is too low.
     */
    @CalledByNative
    public static void scheduleTask(int additionalDelaySeconds, String gcmToken) {
        scheduleTaskInternal(additionalDelaySeconds, false, gcmToken);
    }

    /**
     * Schedules the limitless version of the 'NWake' task for the prefetching service. with reduced
     * delays and no network restrictions (device needs only to be online).
     */
    @CalledByNative
    public static void scheduleTaskLimitless(int additionalDelaySeconds, String gcmToken) {
        scheduleTaskInternal(additionalDelaySeconds, true, gcmToken);
    }

    static Bundle createGCMTokenBundle(String gcmToken) {
        Bundle bundle = new Bundle(1);
        bundle.putString(PrefetchBackgroundTask.GCM_TOKEN_BUNDLE_KEY, gcmToken);
        return bundle;
    }

    private static void scheduleTaskInternal(
            int additionalDelaySeconds, boolean limitlessPrefetching, String gcmToken) {
        final long minimumTimeSeconds =
                (limitlessPrefetching ? 0 : DEFAULT_START_DELAY_SECONDS) + additionalDelaySeconds;
        TaskInfo.Builder taskInfoBuilder =
                TaskInfo.createOneOffTask(TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID,
                                PrefetchBackgroundTask.class,
                                // Minimum time to wait
                                DateUtils.SECOND_IN_MILLIS * minimumTimeSeconds,
                                // Maximum time to wait.  After this interval the event will fire
                                // regardless of whether the conditions are right.
                                DateUtils.DAY_IN_MILLIS * 7)
                        .setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED)
                        .setIsPersisted(true)
                        .setUpdateCurrent(true);

        Bundle bundle = createGCMTokenBundle(gcmToken);
        /* Limitless prefetching eliminates the default wait time but still complies with backoff
         * delays determined by |additionalDelaySeconds|. There's also no restriction on the network
         * type.
         */
        if (limitlessPrefetching) {
            taskInfoBuilder.setRequiredNetworkType(TaskInfo.NetworkType.ANY);
            bundle.putBoolean(PrefetchBackgroundTask.LIMITLESS_BUNDLE_KEY, true);
        }
        taskInfoBuilder.setExtras(bundle);
        BackgroundTaskSchedulerFactory.getScheduler().schedule(
                ContextUtils.getApplicationContext(), taskInfoBuilder.build());
    }

    /**
     * Cancels the default 'NWake' task for the prefetching service.
     */
    @CalledByNative
    public static void cancelTask() {
        BackgroundTaskSchedulerFactory.getScheduler().cancel(
                ContextUtils.getApplicationContext(), TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
    }
}
