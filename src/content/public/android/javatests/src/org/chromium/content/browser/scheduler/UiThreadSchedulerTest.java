// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.scheduler;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.collection.IsIterableContainingInOrder.contains;

import android.os.Handler;
import android.os.HandlerThread;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;
import org.chromium.content.app.ContentMain;
import org.chromium.content_public.browser.BrowserTaskExecutor;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Test class for scheduling on the UI Thread.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class UiThreadSchedulerTest {
    @Rule
    public NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() {
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();
        ThreadUtils.setUiThread(null);
        ThreadUtils.setWillOverrideUiThread(true);
        mUiThread = new HandlerThread("UiThreadForTest");
        mUiThread.start();
        ThreadUtils.setUiThread(mUiThread.getLooper());
        BrowserTaskExecutor.register();
        mHandler = new Handler(mUiThread.getLooper());
    }

    @After
    public void tearDown() {
        mUiThread.quitSafely();
        ThreadUtils.setUiThread(null);
        ThreadUtils.setWillOverrideUiThread(false);
    }

    @Test
    @MediumTest
    public void testSimpleUiThreadPostingBeforeNativeLoaded() throws Exception {
        TaskRunner uiThreadTaskRunner =
                PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.DEFAULT);
        try {
            List<Integer> orderList = new ArrayList<>();
            SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 1);
            SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 2);
            SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 3);
            SchedulerTestHelpers.postTaskAndBlockUntilRun(uiThreadTaskRunner);

            assertThat(orderList, contains(1, 2, 3));
        } finally {
            uiThreadTaskRunner.destroy();
        }
    }

    @Test
    @MediumTest
    public void testUiThreadTaskRunnerMigrationToNative() throws Exception {
        TaskRunner uiThreadTaskRunner =
                PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.DEFAULT);
        try {
            List<Integer> orderList = new ArrayList<>();
            SchedulerTestHelpers.postRecordOrderTask(uiThreadTaskRunner, orderList, 1);

            postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(
                    uiThreadTaskRunner, new Runnable() {
                        @Override
                        public void run() {
                            orderList.add(2);
                        }
                    });
            assertThat(orderList, contains(1, 2));
        } finally {
            uiThreadTaskRunner.destroy();
        }
    }

    @Test
    @MediumTest
    public void testSimpleUiThreadPostingAfterNativeLoaded() throws Exception {
        TaskRunner uiThreadTaskRunner =
                PostTask.createSingleThreadTaskRunner(UiThreadTaskTraits.DEFAULT);
        try {
            startContentMainOnUiThread();

            uiThreadTaskRunner.postTask(new Runnable() {
                @Override
                public void run() {
                    Assert.assertTrue(ThreadUtils.runningOnUiThread());
                }
            });
            SchedulerTestHelpers.postTaskAndBlockUntilRun(uiThreadTaskRunner);
        } finally {
            uiThreadTaskRunner.destroy();
        }
    }

    @Test
    @MediumTest
    public void testTaskNotRunOnUiThreadWithoutUiThreadTaskTraits() throws Exception {
        TaskRunner uiThreadTaskRunner = PostTask.createSingleThreadTaskRunner(new TaskTraits());
        try {
            startContentMainOnUiThread();

            uiThreadTaskRunner.postTask(new Runnable() {
                @Override
                public void run() {
                    Assert.assertFalse(ThreadUtils.runningOnUiThread());
                }
            });

            SchedulerTestHelpers.postTaskAndBlockUntilRun(uiThreadTaskRunner);
        } finally {
            uiThreadTaskRunner.destroy();
        }
    }

    @Test
    @MediumTest
    public void testRunOrPostTask() throws InterruptedException {
        final Object lock = new Object();
        final AtomicBoolean taskExecuted = new AtomicBoolean();
        List<Integer> orderList = new ArrayList<>();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            // We are running on the UI thread now. First, we post a task on the
            // UI thread; it will not run immediately because the UI thread is
            // busy running the current code:
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                orderList.add(1);
                synchronized (lock) {
                    taskExecuted.set(true);
                    lock.notify();
                }
            });
            // Now, we runOrPost a task on the UI thread. We are on the UI thread,
            // so it will run immediately.
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> { orderList.add(2); });
        });
        synchronized (lock) {
            while (!taskExecuted.get()) {
                lock.wait();
            }
        }
        assertThat(orderList, contains(2, 1));
    }

    private void startContentMainOnUiThread() {
        final Object lock = new Object();
        final AtomicBoolean uiThreadInitalized = new AtomicBoolean();

        mHandler.post(new Runnable() {
            @Override
            public void run() {
                try {
                    ContentMain.start(/* startServiceManagerOnly */ true);
                    synchronized (lock) {
                        uiThreadInitalized.set(true);
                        lock.notify();
                    }
                } catch (Exception e) {
                }
            }
        });

        synchronized (lock) {
            try {
                while (!uiThreadInitalized.get()) {
                    lock.wait();
                }
            } catch (InterruptedException ie) {
                ie.printStackTrace();
            }
        }
    }

    private void postRepeatingTaskAndStartNativeSchedulerThenWaitForTaskToRun(
            TaskRunner taskQueue, Runnable taskToRunAfterNativeSchedulerLoaded) throws Exception {
        final Object lock = new Object();
        final AtomicBoolean taskRun = new AtomicBoolean();
        final AtomicBoolean nativeSchedulerStarted = new AtomicBoolean();

        // Post a task that reposts itself until nativeSchedulerStarted is set to true.  This tests
        // that tasks posted before the native library is loaded still run afterwards.
        taskQueue.postTask(new Runnable() {
            @Override
            public void run() {
                if (nativeSchedulerStarted.compareAndSet(true, true)) {
                    taskToRunAfterNativeSchedulerLoaded.run();
                    synchronized (lock) {
                        taskRun.set(true);
                        lock.notify();
                    }
                } else {
                    taskQueue.postTask(this);
                }
            }
        });

        startContentMainOnUiThread();
        nativeSchedulerStarted.set(true);

        synchronized (lock) {
            try {
                while (!taskRun.get()) {
                    lock.wait();
                }
            } catch (InterruptedException ie) {
                ie.printStackTrace();
            }
        }
    }

    private Handler mHandler;
    private HandlerThread mUiThread;
}
