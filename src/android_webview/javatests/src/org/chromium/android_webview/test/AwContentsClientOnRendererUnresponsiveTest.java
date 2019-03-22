// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;

import android.support.test.filters.LargeTest;
import android.view.KeyEvent;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.AwRenderProcessGoneDetail;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.concurrent.TimeUnit;

/**
 * Tests for AwContentsClient.onRenderProcessGone callback.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsClientOnRendererUnresponsiveTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static final String TAG = "AwRendererUnresponsive";

    private static class RendererTransientlyUnresponsiveTestAwContentsClient
            extends TestAwContentsClient {
        private CallbackHelper mUnresponsiveCallbackHelper;
        private CallbackHelper mResponsiveCallbackHelper;

        public RendererTransientlyUnresponsiveTestAwContentsClient() {
            mUnresponsiveCallbackHelper = new CallbackHelper();
            mResponsiveCallbackHelper = new CallbackHelper();
        }

        void transientlyBlockBlinkThread(final AwContents awContents) {
            ThreadUtils.runOnUiThread(() -> {
                // clang-format off
                awContents.evaluateJavaScript(
                        "let t0 = performance.now();\n" +
                        "while(performance.now() < t0 + 6000 /*ms*/) {}",
                        null);
                // clang-format on
            });
        }

        void awaitRecovery() throws Exception {
            // unresponsive signal should occur after 5 seconds, so wait for 6.
            mUnresponsiveCallbackHelper.waitForCallback(0, 1, 6, TimeUnit.SECONDS);
            Assert.assertEquals(1, mUnresponsiveCallbackHelper.getCallCount());
            // blink main thread is transiently blocked for 6 seconds; wait
            // an additional 5 seconds.
            mResponsiveCallbackHelper.waitForCallback(0, 1, 5, TimeUnit.SECONDS);
            Assert.assertEquals(1, mResponsiveCallbackHelper.getCallCount());
        }

        @Override
        public void onRendererResponsive(AwRenderProcess process) {
            mResponsiveCallbackHelper.notifyCalled();
        }

        @Override
        public void onRendererUnresponsive(AwRenderProcess process) {
            // onRendererResponsive should not have been called yet.
            Assert.assertEquals(0, mResponsiveCallbackHelper.getCallCount());
            mUnresponsiveCallbackHelper.notifyCalled();
        }
    }

    private static class RendererUnresponsiveTestAwContentsClient extends TestAwContentsClient {
        // The renderer unresponsive callback should be called repeatedly. We will wait for two
        // callbacks.
        static final int UNRESPONSIVE_CALLBACK_COUNT = 2;

        private CallbackHelper mUnresponsiveCallbackHelper;
        private CallbackHelper mTerminatedCallbackHelper;

        public RendererUnresponsiveTestAwContentsClient() {
            mUnresponsiveCallbackHelper = new CallbackHelper();
            mTerminatedCallbackHelper = new CallbackHelper();
        }

        void permanentlyBlockBlinkThread(final AwContents awContents) {
            ThreadUtils.runOnUiThread(() -> { awContents.evaluateJavaScript("while(1);", null); });
        }

        void awaitRendererTermination() throws Exception {
            // The input ack timeout is 5 seconds, and the unresponsive callback should retrigger at
            // that interval, so wait for the expected multiple of 5 seconds, plus one second
            // leeway.
            mUnresponsiveCallbackHelper.waitForCallback(
                    0, 2, 5 * UNRESPONSIVE_CALLBACK_COUNT + 1, TimeUnit.SECONDS);
            Assert.assertEquals(2, mUnresponsiveCallbackHelper.getCallCount());

            mTerminatedCallbackHelper.waitForCallback(0, 1, 5, TimeUnit.SECONDS);
            Assert.assertEquals(1, mTerminatedCallbackHelper.getCallCount());
        }

        @Override
        public boolean onRenderProcessGone(AwRenderProcessGoneDetail detail) {
            mTerminatedCallbackHelper.notifyCalled();
            return true;
        }

        @Override
        public void onRendererUnresponsive(AwRenderProcess process) {
            mUnresponsiveCallbackHelper.notifyCalled();
            if (mUnresponsiveCallbackHelper.getCallCount() == UNRESPONSIVE_CALLBACK_COUNT) {
                process.terminate();
            }
        }
    }

    private void sendInputEvent(final AwContents awContents) {
        ThreadUtils.runOnUiThread(() -> {
            awContents.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        });
    }
    @Test
    @Feature({"AndroidWebView"})
    @LargeTest
    @OnlyRunIn(MULTI_PROCESS)
    public void testOnRendererUnresponsive() throws Throwable {
        RendererUnresponsiveTestAwContentsClient contentsClient =
                new RendererUnresponsiveTestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testView.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        mActivityTestRule.loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        contentsClient.permanentlyBlockBlinkThread(awContents);
        // Sending a key event while the renderer is unresponsive will cause onRendererUnresponsive
        // to be called.
        sendInputEvent(awContents);
        contentsClient.awaitRendererTermination();
    }

    @Test
    @Feature({"AndroidWebView"})
    @LargeTest
    public void testTransientUnresponsiveness() throws Throwable {
        RendererTransientlyUnresponsiveTestAwContentsClient contentsClient =
                new RendererTransientlyUnresponsiveTestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testView.getAwContents();

        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        mActivityTestRule.loadUrlSync(awContents, contentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        contentsClient.transientlyBlockBlinkThread(awContents);
        sendInputEvent(awContents);
        contentsClient.awaitRecovery();
    }
}
