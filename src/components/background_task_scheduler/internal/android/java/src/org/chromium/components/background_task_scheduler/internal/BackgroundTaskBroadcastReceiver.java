// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkInfo;
import android.os.BatteryManager;
import android.os.Build;
import android.os.PowerManager;
import android.text.format.DateUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Starts running the BackgroundTask at the specified triggering time.
 *
 * Receives the information through a broadcast, which is synchronous in the Main thread. The
 * execution of the task will be detached to a best effort task.
 */
public class BackgroundTaskBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "BkgrdTaskBR";
    private static final String WAKELOCK_TAG = "Chromium:" + TAG;

    // Wakelock is only held for 3 minutes, in order to be consistent with the restrictions of
    // the GcmTaskService:
    // https://developers.google.com/android/reference/com/google/android/gms/gcm/GcmTaskService.
    // Here the waiting is done for only 90% of this time.
    private static final long MAX_TIMEOUT_MS = 162 * DateUtils.SECOND_IN_MILLIS;

    private static class TaskExecutor implements BackgroundTask.TaskFinishedCallback {
        private final Context mContext;
        private final PowerManager.WakeLock mWakeLock;
        private final TaskParameters mTaskParams;
        private final BackgroundTask mBackgroundTask;

        private boolean mHasExecuted;

        public TaskExecutor(Context context, PowerManager.WakeLock wakeLock,
                TaskParameters taskParams, BackgroundTask backgroundTask) {
            mContext = context;
            mWakeLock = wakeLock;
            mTaskParams = taskParams;
            mBackgroundTask = backgroundTask;
        }

        public void execute() {
            ThreadUtils.assertOnUiThread();

            boolean needsBackground = mBackgroundTask.onStartTask(mContext, mTaskParams, this);
            BackgroundTaskSchedulerUma.getInstance().reportTaskStarted(mTaskParams.getTaskId());
            if (!needsBackground) return;

            PostTask.postDelayedTask(UiThreadTaskTraits.BEST_EFFORT, this::timeout, MAX_TIMEOUT_MS);
        }

        @Override
        public void taskFinished(boolean needsReschedule) {
            PostTask.postTask(UiThreadTaskTraits.BEST_EFFORT, () -> finished(needsReschedule));
        }

        private void timeout() {
            ThreadUtils.assertOnUiThread();
            if (mHasExecuted) return;
            mHasExecuted = true;

            Log.w(TAG, "Task execution failed. Task timed out.");
            BackgroundTaskSchedulerUma.getInstance().reportTaskStopped(mTaskParams.getTaskId());

            boolean reschedule = mBackgroundTask.onStopTask(mContext, mTaskParams);
            if (reschedule) {
                BackgroundTaskSchedulerUma.getInstance().reportTaskRescheduled();
                mBackgroundTask.reschedule(mContext);
            }
        }

        private void finished(boolean reschedule) {
            ThreadUtils.assertOnUiThread();
            if (mHasExecuted) return;
            mHasExecuted = true;

            if (reschedule) {
                BackgroundTaskSchedulerUma.getInstance().reportTaskRescheduled();
                mBackgroundTask.reschedule(mContext);
            }
            // TODO(crbug.com/970160): Add UMA to record how long the tasks need to complete.
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        final TaskParameters taskParams =
                BackgroundTaskSchedulerAlarmManager.getTaskParametersFromIntent(intent);
        if (taskParams == null) {
            Log.w(TAG, "Failed to retrieve task parameters.");
            return;
        }

        int taskId = taskParams.getTaskId();
        ScheduledTaskProto.ScheduledTask scheduledTask =
                BackgroundTaskSchedulerPrefs.getScheduledTask(taskId);
        if (scheduledTask == null) {
            Log.e(TAG, "Cannot get information about task with task ID " + taskId);
            return;
        }

        // Only continue if network requirements match network status.
        if (!networkRequirementsMet(context, taskId,
                    convertToTaskInfoNetworkType(scheduledTask.getRequiredNetworkType()))) {
            Log.w(TAG,
                    "Failed to start task. Network requirements not satisfied for task with task ID"
                            + taskId);
            return;
        }

        // Check if battery requirements match.
        if (!batteryRequirementsMet(context, taskId, scheduledTask.getRequiresCharging())) {
            Log.w(TAG,
                    "Failed to start task. Battery requirements not satisfied for task with task ID"
                            + taskId);
            return;
        }

        final BackgroundTask backgroundTask =
                BackgroundTaskSchedulerFactoryInternal.getBackgroundTaskFromTaskId(taskId);
        if (backgroundTask == null) {
            Log.w(TAG, "Failed to start task. Could not instantiate BackgroundTask class.");
            // Cancel task if the BackgroundTask class is not found anymore. We assume this means
            // that the task has been deprecated.
            BackgroundTaskSchedulerFactoryInternal.getScheduler().cancel(
                    ContextUtils.getApplicationContext(), taskParams.getTaskId());
            return;
        }

        // Keep the CPU on through a wake lock.
        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        PowerManager.WakeLock wakeLock =
                powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, WAKELOCK_TAG);
        wakeLock.acquire(MAX_TIMEOUT_MS);

        TaskExecutor taskExecutor = new TaskExecutor(context, wakeLock, taskParams, backgroundTask);
        PostTask.postTask(UiThreadTaskTraits.BEST_EFFORT, taskExecutor::execute);
    }

    private boolean networkRequirementsMet(Context context, int taskId, int requiredNetworkType) {
        if (requiredNetworkType == TaskInfo.NetworkType.NONE) return true;

        ConnectivityManager connectivityManager =
                (ConnectivityManager) context.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Network network = connectivityManager.getActiveNetwork();
            if (requiredNetworkType == TaskInfo.NetworkType.ANY) return (network != null);
        } else {
            NetworkInfo networkInfo = connectivityManager.getActiveNetworkInfo();
            if (requiredNetworkType == TaskInfo.NetworkType.ANY) return (networkInfo != null);
        }

        return (!connectivityManager.isActiveNetworkMetered());
    }

    private boolean batteryRequirementsMet(Context context, int taskId, boolean requiresCharging) {
        if (!requiresCharging) return true;
        BatteryManager batteryManager =
                (BatteryManager) context.getSystemService(Context.BATTERY_SERVICE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return batteryManager.isCharging();
        }

        IntentFilter intentFilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryStatus = context.registerReceiver(null, intentFilter);
        int status = batteryStatus.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
        return status == BatteryManager.BATTERY_STATUS_CHARGING
                || status == BatteryManager.BATTERY_STATUS_FULL;
    }

    private @TaskInfo.NetworkType int convertToTaskInfoNetworkType(
            ScheduledTaskProto.ScheduledTask.RequiredNetworkType networkType) {
        switch (networkType) {
            case NONE:
                return TaskInfo.NetworkType.NONE;
            case ANY:
                return TaskInfo.NetworkType.ANY;
            case UNMETERED:
                return TaskInfo.NetworkType.UNMETERED;
            default:
                assert false : "Incorrect value of RequiredNetworkType";
                return -1;
        }
    }
}
