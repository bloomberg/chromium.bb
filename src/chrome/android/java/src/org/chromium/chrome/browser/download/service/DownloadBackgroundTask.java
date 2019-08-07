// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.service;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask.StartBeforeNativeResult;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.download.DownloadTaskType;
import org.chromium.components.download.internal.BatteryStatusListenerAndroid;
import org.chromium.content_public.browser.BrowserStartupController;

/**
 * Entry point for the download service to perform desired action when the task is fired by the
 * scheduler.
 */
@JNINamespace("download::android")
public class DownloadBackgroundTask extends NativeBackgroundTask {
    @DownloadTaskType
    private int mCurrentTaskType;

    // Whether only service manager is required to start.
    private boolean mStartsServiceManagerOnly;

    @Override
    protected @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        boolean requiresCharging = taskParameters.getExtras().getBoolean(
                DownloadTaskScheduler.EXTRA_BATTERY_REQUIRES_CHARGING);
        int optimalBatteryPercentage = taskParameters.getExtras().getInt(
                DownloadTaskScheduler.EXTRA_OPTIMAL_BATTERY_PERCENTAGE);
        mCurrentTaskType = taskParameters.getExtras().getInt(DownloadTaskScheduler.EXTRA_TASK_TYPE);
        // Return value from DownloadUtils.shouldStartServiceManagerOnly() could change during
        // native initialization, store it first.
        mStartsServiceManagerOnly =
                mCurrentTaskType == DownloadTaskType.DOWNLOAD_AUTO_RESUMPTION_TASK
                && DownloadUtils.shouldStartServiceManagerOnly();
        // Reschedule if minimum battery level is not satisfied.
        if (!requiresCharging
                && BatteryStatusListenerAndroid.getBatteryPercentage() < optimalBatteryPercentage) {
            return StartBeforeNativeResult.RESCHEDULE;
        }

        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, final TaskFinishedCallback callback) {
        // In case of future upgrades, we would need to build an intent for the old version and
        // validate that this code still works. This would require decoupling this immediate class
        // from native as well.

        assert BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isStartupSuccessfullyCompleted()
                || mStartsServiceManagerOnly;
        Profile profile = mStartsServiceManagerOnly
                ? null
                : Profile.getLastUsedProfile().getOriginalProfile();
        nativeStartBackgroundTask(profile, mCurrentTaskType,
                needsReschedule -> callback.taskFinished(needsReschedule));
    }

    @Override
    protected boolean supportsServiceManagerOnly() {
        return mStartsServiceManagerOnly;
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        @DownloadTaskType
        int taskType = taskParameters.getExtras().getInt(DownloadTaskScheduler.EXTRA_TASK_TYPE);

        Profile profile = mStartsServiceManagerOnly
                ? null
                : Profile.getLastUsedProfile().getOriginalProfile();
        return nativeStopBackgroundTask(profile, taskType);
    }

    @Override
    public void reschedule(Context context) {
        DownloadTaskScheduler.rescheduleAllTasks();
    }

    private native void nativeStartBackgroundTask(
            Profile profile, int taskType, Callback<Boolean> callback);
    private native boolean nativeStopBackgroundTask(Profile profile, int taskType);
}
