// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.library_loader.LoaderErrors;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BackgroundTaskScheduler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NativeBackgroundTaskTest {
    private enum InitializerSetup {
        SUCCESS,
        FAILURE,
        EXCEPTION,
    }

    private static class LazyTaskParameters {
        static final TaskParameters INSTANCE = TaskParameters.create(TaskIds.TEST).build();
    }

    private static TaskParameters getTaskParameters() {
        return LazyTaskParameters.INSTANCE;
    }

    private static class TestBrowserStartupController implements BrowserStartupController {
        private boolean mStartupSucceeded;
        private int mCallCount;

        @Override
        public void startBrowserProcessesAsync(boolean startGpuProcess,
                boolean startServiceManagerOnly, final StartupCallback callback)
                throws ProcessInitException {}

        @Override
        public void startBrowserProcessesSync(boolean singleProcess) throws ProcessInitException {}

        @Override
        public boolean isStartupSuccessfullyCompleted() {
            mCallCount++;
            return mStartupSucceeded;
        }

        @Override
        public void addStartupCompletedObserver(StartupCallback callback) {}

        @Override
        public void initChromiumBrowserProcessForTests() {}
        public void setIsStartupSuccessfullyCompleted(boolean flag) {
            mStartupSucceeded = flag;
        }
        public int completedCallCount() {
            return mCallCount;
        }
    }

    private TestBrowserStartupController mBrowserStartupController;
    private TaskFinishedCallback mCallback;
    private TestNativeBackgroundTask mTask;

    @Mock
    private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Captor
    ArgumentCaptor<BrowserParts> mBrowserParts;

    private static class TaskFinishedCallback implements BackgroundTask.TaskFinishedCallback {
        private boolean mWasCalled;
        private boolean mNeedsReschedule;
        private CountDownLatch mCallbackLatch;

        TaskFinishedCallback() {
            mCallbackLatch = new CountDownLatch(1);
        }

        @Override
        public void taskFinished(boolean needsReschedule) {
            mNeedsReschedule = needsReschedule;
            mWasCalled = true;
            mCallbackLatch.countDown();
        }

        boolean wasCalled() {
            return mWasCalled;
        }

        boolean needsRescheduling() {
            return mNeedsReschedule;
        }

        boolean waitOnCallback() {
            return waitOnLatch(mCallbackLatch);
        }
    }

    private static class TestNativeBackgroundTask extends NativeBackgroundTask {
        @StartBeforeNativeResult
        private int mStartBeforeNativeResult;
        private boolean mWasOnStartTaskWithNativeCalled;
        private boolean mNeedsReschedulingAfterStop;
        private CountDownLatch mStartWithNativeLatch;
        private boolean mWasOnStopTaskWithNativeCalled;
        private boolean mWasOnStopTaskBeforeNativeLoadedCalled;
        private BrowserStartupController mBrowserStartupController;

        public TestNativeBackgroundTask(BrowserStartupController controller) {
            mBrowserStartupController = controller;
            mWasOnStartTaskWithNativeCalled = false;
            mStartBeforeNativeResult = StartBeforeNativeResult.LOAD_NATIVE;
            mNeedsReschedulingAfterStop = false;
            mStartWithNativeLatch = new CountDownLatch(1);
        }

        @Override
        protected int onStartTaskBeforeNativeLoaded(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            return mStartBeforeNativeResult;
        }

        @Override
        protected void onStartTaskWithNative(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            assertEquals(RuntimeEnvironment.application, context);
            assertEquals(getTaskParameters(), taskParameters);
            mWasOnStartTaskWithNativeCalled = true;
            mStartWithNativeLatch.countDown();
        }

        @Override
        protected boolean onStopTaskBeforeNativeLoaded(
                Context context, TaskParameters taskParameters) {
            mWasOnStopTaskBeforeNativeLoadedCalled = true;
            return mNeedsReschedulingAfterStop;
        }

        @Override
        protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
            mWasOnStopTaskWithNativeCalled = true;
            return mNeedsReschedulingAfterStop;
        }

        @Override
        public void reschedule(Context context) {}

        boolean waitOnStartWithNativeCallback() {
            return waitOnLatch(mStartWithNativeLatch);
        }

        boolean wasOnStartTaskWithNativeCalled() {
            return mWasOnStartTaskWithNativeCalled;
        }

        boolean wasOnStopTaskWithNativeCalled() {
            return mWasOnStopTaskWithNativeCalled;
        }

        boolean wasOnStopTaskBeforeNativeLoadedCalled() {
            return mWasOnStopTaskBeforeNativeLoadedCalled;
        }

        void setStartTaskBeforeNativeResult(@StartBeforeNativeResult int result) {
            mStartBeforeNativeResult = result;
        }

        void setNeedsReschedulingAfterStop(boolean needsReschedulingAfterStop) {
            mNeedsReschedulingAfterStop = needsReschedulingAfterStop;
        }

        @Override
        public BrowserStartupController getBrowserStartupController() {
            return mBrowserStartupController;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mBrowserStartupController = new TestBrowserStartupController();
        mCallback = new TaskFinishedCallback();
        mTask = new TestNativeBackgroundTask(mBrowserStartupController);
        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);
        ChromeBrowserInitializer.setBrowserStartupControllerForTesting(mBrowserStartupController);
    }

    private void setUpChromeBrowserInitializer(InitializerSetup setup) {
        doNothing().when(mChromeBrowserInitializer).handlePreNativeStartup(any(BrowserParts.class));
        try {
            switch (setup) {
                case SUCCESS:
                    doAnswer(new Answer<Void>() {
                        @Override
                        public Void answer(InvocationOnMock invocation) {
                            mBrowserParts.getValue().finishNativeInitialization();
                            return null;
                        }
                    })
                            .when(mChromeBrowserInitializer)
                            .handlePostNativeStartup(eq(true), mBrowserParts.capture());
                    break;
                case FAILURE:
                    doAnswer(new Answer<Void>() {
                        @Override
                        public Void answer(InvocationOnMock invocation) {
                            mBrowserParts.getValue().onStartupFailure();
                            return null;
                        }
                    })
                            .when(mChromeBrowserInitializer)
                            .handlePostNativeStartup(eq(true), mBrowserParts.capture());
                    break;
                case EXCEPTION:
                    doThrow(new ProcessInitException(
                                    LoaderErrors.LOADER_ERROR_NATIVE_LIBRARY_LOAD_FAILED))
                            .when(mChromeBrowserInitializer)
                            .handlePostNativeStartup(eq(true), any(BrowserParts.class));
                    break;
                default:
                    assert false;
            }
        } catch (ProcessInitException e) {
            // Exception ignored, as try-catch is required by language.
        }
    }

    private void verifyStartupCalls(int expectedPreNativeCalls, int expectedPostNativeCalls) {
        try {
            verify(mChromeBrowserInitializer, times(expectedPreNativeCalls))
                    .handlePreNativeStartup(any(BrowserParts.class));
            verify(mChromeBrowserInitializer, times(expectedPostNativeCalls))
                    .handlePostNativeStartup(eq(true), any(BrowserParts.class));
        } catch (ProcessInitException e) {
            // Exception ignored, as try-catch is required by language.
        }
    }

    private static boolean waitOnLatch(CountDownLatch latch) {
        try {
            // All tests are expected to get it done much faster
            return latch.await(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            return false;
        }
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_Done_BeforeNativeLoaded() {
        mTask.setStartTaskBeforeNativeResult(NativeBackgroundTask.StartBeforeNativeResult.DONE);
        assertFalse(
                mTask.onStartTask(RuntimeEnvironment.application, getTaskParameters(), mCallback));

        assertEquals(0, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(0, 0);
        assertFalse(mTask.wasOnStartTaskWithNativeCalled());
        assertFalse(mCallback.wasCalled());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_Reschedule_BeforeNativeLoaded() {
        mTask.setStartTaskBeforeNativeResult(
                NativeBackgroundTask.StartBeforeNativeResult.RESCHEDULE);
        assertTrue(
                mTask.onStartTask(RuntimeEnvironment.application, getTaskParameters(), mCallback));

        assertTrue(mCallback.waitOnCallback());
        assertEquals(0, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(0, 0);
        assertFalse(mTask.wasOnStartTaskWithNativeCalled());
        assertTrue(mCallback.wasCalled());
        assertTrue(mCallback.needsRescheduling());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_NativeAlreadyLoaded() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(true);
        mTask.onStartTask(RuntimeEnvironment.application, getTaskParameters(), mCallback);

        assertTrue(mTask.waitOnStartWithNativeCallback());
        assertEquals(1, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(0, 0);
        assertTrue(mTask.wasOnStartTaskWithNativeCalled());
        assertFalse(mCallback.wasCalled());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_NativeInitialization_Success() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(false);
        setUpChromeBrowserInitializer(InitializerSetup.SUCCESS);
        mTask.onStartTask(RuntimeEnvironment.application, getTaskParameters(), mCallback);

        assertTrue(mTask.waitOnStartWithNativeCallback());
        assertEquals(1, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(1, 1);
        assertTrue(mTask.wasOnStartTaskWithNativeCalled());
        assertFalse(mCallback.wasCalled());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_NativeInitialization_Failure() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(false);
        setUpChromeBrowserInitializer(InitializerSetup.FAILURE);
        mTask.onStartTask(RuntimeEnvironment.application, getTaskParameters(), mCallback);

        assertTrue(mCallback.waitOnCallback());
        assertEquals(1, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(1, 1);
        assertFalse(mTask.wasOnStartTaskWithNativeCalled());
        assertTrue(mCallback.wasCalled());
        assertTrue(mCallback.needsRescheduling());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStartTask_NativeInitialization_Throws() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(false);
        setUpChromeBrowserInitializer(InitializerSetup.EXCEPTION);
        mTask.onStartTask(RuntimeEnvironment.application, getTaskParameters(), mCallback);

        assertTrue(mCallback.waitOnCallback());
        assertEquals(1, mBrowserStartupController.completedCallCount());
        verifyStartupCalls(1, 1);
        assertFalse(mTask.wasOnStartTaskWithNativeCalled());
        assertTrue(mCallback.wasCalled());
        assertTrue(mCallback.needsRescheduling());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStopTask_BeforeNativeLoaded_NeedsRescheduling() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(false);
        mTask.onStartTask(RuntimeEnvironment.application, getTaskParameters(), mCallback);
        mTask.setNeedsReschedulingAfterStop(true);

        assertTrue(mTask.onStopTask(RuntimeEnvironment.application, getTaskParameters()));
        assertTrue(mTask.wasOnStopTaskBeforeNativeLoadedCalled());
        assertFalse(mTask.wasOnStopTaskWithNativeCalled());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStopTask_BeforeNativeLoaded_DoesntNeedRescheduling() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(false);
        mTask.onStartTask(RuntimeEnvironment.application, getTaskParameters(), mCallback);
        mTask.setNeedsReschedulingAfterStop(false);

        assertFalse(mTask.onStopTask(RuntimeEnvironment.application, getTaskParameters()));
        assertTrue(mTask.wasOnStopTaskBeforeNativeLoadedCalled());
        assertFalse(mTask.wasOnStopTaskWithNativeCalled());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStopTask_NativeLoaded_NeedsRescheduling() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(true);
        mTask.onStartTask(RuntimeEnvironment.application, getTaskParameters(), mCallback);
        mTask.setNeedsReschedulingAfterStop(true);

        assertTrue(mTask.onStopTask(RuntimeEnvironment.application, getTaskParameters()));
        assertFalse(mTask.wasOnStopTaskBeforeNativeLoadedCalled());
        assertTrue(mTask.wasOnStopTaskWithNativeCalled());
    }

    @Test
    @Feature("BackgroundTaskScheduler")
    public void testOnStopTask_NativeLoaded_DoesntNeedRescheduling() {
        mBrowserStartupController.setIsStartupSuccessfullyCompleted(true);
        mTask.onStartTask(RuntimeEnvironment.application, getTaskParameters(), mCallback);
        mTask.setNeedsReschedulingAfterStop(false);

        assertFalse(mTask.onStopTask(RuntimeEnvironment.application, getTaskParameters()));
        assertFalse(mTask.wasOnStopTaskBeforeNativeLoadedCalled());
        assertTrue(mTask.wasOnStopTaskWithNativeCalled());
    }
}
