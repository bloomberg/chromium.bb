// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ShadowDeviceConditions;
import org.chromium.chrome.browser.background_task_scheduler.NativeBackgroundTask;
import org.chromium.chrome.test.support.DisableHistogramsRule;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;

import java.util.HashMap;

/**
 * Unit tests for BackgroundSyncBackgroundTask.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowDeviceConditions.class})
public class BackgroundSyncBackgroundTaskTest {
    private static final String IS_LOW_END_DEVICE_SWITCH =
            "--" + BaseSwitches.ENABLE_LOW_END_DEVICE_MODE;

    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    @Rule
    public JniMocker mocker = new JniMocker();

    private Bundle mTaskExtras;
    private long mTaskTime;

    @Mock
    private BackgroundSyncBackgroundTask.Natives mNativeMock;
    @Mock
    private BackgroundTaskScheduler mTaskScheduler;
    @Mock
    private BackgroundTask.TaskFinishedCallback mTaskFinishedCallback;
    @Mock
    private Callback<Boolean> mInternalBooleanCallback;
    @Captor
    private ArgumentCaptor<TaskInfo> mTaskInfo;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);

        HashMap<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.BACKGROUND_TASK_SCHEDULER_FOR_BACKGROUND_SYNC, true);
        ChromeFeatureList.setTestFeatures(features);
        mTaskExtras = new Bundle();

        doReturn(true)
                .when(mTaskScheduler)
                .schedule(eq(RuntimeEnvironment.application), mTaskInfo.capture());

        ShadowDeviceConditions.setCurrentNetworkConnectionType(ConnectionType.CONNECTION_NONE);

        // Run tests as a low-end device.
        CommandLine.init(new String[] {"testcommand", IS_LOW_END_DEVICE_SWITCH});

        mocker.mock(BackgroundSyncBackgroundTaskJni.TEST_HOOKS, mNativeMock);
    }

    @After
    public void tearDown() throws Exception {
        // Clean up static state for subsequent Robolectric tests.
        CommandLine.reset();
        SysUtils.resetForTesting();
    }

    @Test
    @Feature("BackgroundSync")
    public void testNetworkConditions_NoNetwork() {
        // The test has been set up with no network by default.
        TaskParameters params = TaskParameters.create(TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID)
                                        .addExtras(mTaskExtras)
                                        .build();

        int result = new BackgroundSyncBackgroundTask().onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, mTaskFinishedCallback);
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE, result);

        // TaskFinishedCallback callback is only called once native code has
        // finished processing pending Background Sync registrations.
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
    }

    @Test
    @Feature("BackgroundSync")
    public void testNetworkConditions_Wifi() {
        ShadowDeviceConditions.setCurrentNetworkConnectionType(ConnectionType.CONNECTION_WIFI);
        TaskParameters params = TaskParameters.create(TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID)
                                        .addExtras(mTaskExtras)
                                        .build();

        int result = new BackgroundSyncBackgroundTask().onStartTaskBeforeNativeLoaded(
                RuntimeEnvironment.application, params, mTaskFinishedCallback);
        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE, result);

        // TaskFinishedCallback callback is only called once native code has
        // finished processing pending Background Sync registrations.
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
    }

    @Test
    @Feature("BackgroundSync")
    public void testOnStartTaskWithNative() {
        TaskParameters params = TaskParameters.create(TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID)
                                        .addExtras(mTaskExtras)
                                        .build();

        new BackgroundSyncBackgroundTask().onStartTaskWithNative(
                RuntimeEnvironment.application, params, mTaskFinishedCallback);

        verify(mNativeMock).fireBackgroundSyncEvents(any(Runnable.class));
        verify(mTaskFinishedCallback, times(0)).taskFinished(anyBoolean());
        verify(mTaskScheduler, times(0)).schedule(any(Context.class), any(TaskInfo.class));
    }
}
