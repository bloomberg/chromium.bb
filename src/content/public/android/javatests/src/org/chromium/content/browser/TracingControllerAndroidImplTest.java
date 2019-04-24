// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.os.ConditionVariable;
import android.support.test.filters.MediumTest;
import android.util.Pair;

import org.hamcrest.CoreMatchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_shell_apk.ContentShellActivity;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;

import java.io.File;
import java.io.FileInputStream;
import java.util.Arrays;

/**
 * Test suite for TracingControllerAndroidImpl.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TracingControllerAndroidImplTest {
    @Rule
    public ContentShellActivityTestRule mActivityTestRule = new ContentShellActivityTestRule();

    private static final long TIMEOUT_MILLIS = scaleTimeout(30 * 1000);

    @Test
    @MediumTest
    @Feature({"GPU"})
    @DisableIf.Build(sdk_is_less_than = 21, message = "crbug.com/899894")
    public void testTraceFileCreation() throws Exception {
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl("about:blank");
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        final TracingControllerAndroidImpl tracingController =
                new TracingControllerAndroidImpl(activity);
        Assert.assertFalse(tracingController.isTracing());
        Assert.assertNull(tracingController.getOutputPath());

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(tracingController.startTracing(true, "*", "record-until-full"));
        });

        Assert.assertTrue(tracingController.isTracing());
        File file = new File(tracingController.getOutputPath());
        Assert.assertTrue(file.getName().startsWith("chrome-profile-results"));

        ThreadUtils.runOnUiThreadBlocking(() -> tracingController.stopTracing(null));

        // The tracer stops asynchronously, because it needs to wait for native code to flush and
        // close the output file. Give it a little time.
        CriteriaHelper.pollInstrumentationThread(() -> !tracingController.isTracing(),
                "Timed out waiting for tracing to stop.", TIMEOUT_MILLIS, 100);

        // It says it stopped, so it should have written the output file.
        Assert.assertTrue(file.exists());
        Assert.assertTrue(file.delete());
        ThreadUtils.runOnUiThreadBlocking(() -> tracingController.destroy());
    }

    private class TestCallback<T> implements Callback<T> {
        @Override
        public void onResult(T result) {
            mResult = result;
            mWasCalled.open();
        }

        public ConditionVariable mWasCalled = new ConditionVariable();
        public T mResult;
    }

    @Test
    @MediumTest
    @Feature({"GPU"})
    public void testGetKnownCategories() throws Exception {
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl("about:blank");
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        final TracingControllerAndroidImpl tracingController =
                new TracingControllerAndroidImpl(activity);
        Assert.assertFalse(tracingController.isTracing());

        TestCallback<String[]> callback = new TestCallback<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(tracingController.getKnownCategories(callback)); });

        Assert.assertTrue(callback.mWasCalled.block(TIMEOUT_MILLIS));
        Assert.assertThat(Arrays.asList(callback.mResult), CoreMatchers.hasItem("toplevel"));
        ThreadUtils.runOnUiThreadBlocking(() -> tracingController.destroy());
    }

    @Test
    @MediumTest
    @Feature({"GPU"})
    public void testBufferUsage() throws Exception {
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl("about:blank");
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        final TracingControllerAndroidImpl tracingController =
                new TracingControllerAndroidImpl(activity);
        Assert.assertFalse(tracingController.isTracing());

        // This should obtain an empty buffer usage, since we aren't tracing.
        TestCallback<Pair<Float, Long>> callback = new TestCallback<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(tracingController.getTraceBufferUsage(callback)); });

        Assert.assertTrue(callback.mWasCalled.block(TIMEOUT_MILLIS));
        Assert.assertEquals(0f, (double) callback.mResult.first, 0.5f);
        Assert.assertEquals(0, (long) callback.mResult.second);
        ThreadUtils.runOnUiThreadBlocking(() -> tracingController.destroy());
    }

    @Test
    @MediumTest
    @Feature({"GPU"})
    @DisableIf.Build(sdk_is_less_than = 21, message = "crbug.com/899894")
    public void testStopCallbackAndCompression() throws Exception {
        ContentShellActivity activity = mActivityTestRule.launchContentShellWithUrl("about:blank");
        mActivityTestRule.waitForActiveShellToBeDoneLoading();

        final TracingControllerAndroidImpl tracingController =
                new TracingControllerAndroidImpl(activity);
        Assert.assertFalse(tracingController.isTracing());
        Assert.assertNull(tracingController.getOutputPath());

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(
                    tracingController.startTracing(null, true, "*", "record-until-full", true));
        });

        Assert.assertTrue(tracingController.isTracing());
        File file = new File(tracingController.getOutputPath());

        TestCallback<Void> callback = new TestCallback<>();
        ThreadUtils.runOnUiThreadBlocking(() -> tracingController.stopTracing(callback));

        // Callback should be run once stopped.
        Assert.assertTrue(callback.mWasCalled.block(TIMEOUT_MILLIS));

        // Should have written the output file, which should start with the gzip header.
        Assert.assertTrue(file.exists());
        FileInputStream stream = new FileInputStream(file);
        byte[] bytes = new byte[2];
        Assert.assertEquals(2, stream.read(bytes));
        Assert.assertEquals((byte) 0x1f, bytes[0]);
        Assert.assertEquals((byte) 0x8b, bytes[1]);
        Assert.assertTrue(file.delete());
        ThreadUtils.runOnUiThreadBlocking(() -> tracingController.destroy());
    }
}
