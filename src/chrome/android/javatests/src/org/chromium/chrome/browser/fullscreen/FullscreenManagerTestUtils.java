// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.os.SystemClock;

import org.junit.Assert;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Utility methods for testing the {@link ChromeFullscreenManager}.
 */
public class FullscreenManagerTestUtils {
    /**
     * Scrolls the underlying web page to show or hide the browser controls.
     * @param testRule The test rule for the currently running test.
     * @param show Whether the browser controls should be shown.
     */
    public static void scrollBrowserControls(ChromeActivityTestRule testRule, boolean show) {
        ChromeFullscreenManager fullscreenManager = testRule.getActivity().getFullscreenManager();
        int browserControlsHeight = fullscreenManager.getTopControlsHeight();

        waitForPageToBeScrollable(testRule.getActivity().getActivityTab());

        float dragX = 50f;
        // Use a larger scroll range than the height of the browser controls to ensure we overcome
        // the delay in a scroll start being sent.
        float dragStartY = browserControlsHeight * 3;
        float dragEndY = dragStartY - browserControlsHeight * 2;
        int expectedPosition = -browserControlsHeight;
        if (show) {
            expectedPosition = 0;
            float tempDragStartY = dragStartY;
            dragStartY = dragEndY;
            dragEndY = tempDragStartY;
        }
        long downTime = SystemClock.uptimeMillis();
        TouchCommon.dragStart(testRule.getActivity(), dragX, dragStartY, downTime);
        TouchCommon.dragTo(
                testRule.getActivity(), dragX, dragX, dragStartY, dragEndY, 100, downTime);
        TouchCommon.dragEnd(testRule.getActivity(), dragX, dragEndY, downTime);
        waitForBrowserControlsPosition(testRule, expectedPosition);
    }

    /**
     * Waits for the browser controls to reach the specified position.
     * @param testRule The test rule for the currently running test.
     * @param position The desired top controls offset.
     */
    public static void waitForBrowserControlsPosition(
            ChromeActivityTestRule testRule, int position) {
        final ChromeFullscreenManager fullscreenManager =
                testRule.getActivity().getFullscreenManager();
        CriteriaHelper.pollUiThread(Criteria.equals(position, new Callable<Integer>() {
            @Override
            public Integer call() {
                return fullscreenManager.getTopControlOffset();
            }
        }));
    }

    /**
     * Waits for the base page to be scrollable.
     * @param tab The current activity tab.
     */
    public static void waitForPageToBeScrollable(final Tab tab) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return RenderCoordinates.fromWebContents(tab.getWebContents())
                               .getContentHeightPixInt()
                        > tab.getContentView().getHeight();
            }
        });
    }

    /**
     * Waits for the browser controls to be moveable by user gesture.
     *
     * This function requires the browser controls to start fully visible. Then it ensures that
     * at some point the controls can be moved by user gesture.  It will then fully cycle the top
     * controls to entirely hidden and back to fully shown.
     *
     * @param testRule The test rule for the currently running test.
     * @param tab The current activity tab.
     */
    public static void waitForBrowserControlsToBeMoveable(
            ChromeActivityTestRule testRule, final Tab tab) throws InterruptedException {
        waitForBrowserControlsPosition(testRule, 0);

        final CallbackHelper contentMovedCallback = new CallbackHelper();
        final ChromeFullscreenManager fullscreenManager =
                testRule.getActivity().getFullscreenManager();
        final float initialVisibleContentOffset = fullscreenManager.getTopVisibleContentOffset();

        fullscreenManager.addListener(new FullscreenListener() {
            @Override
            public void onControlsOffsetChanged(
                    int topOffset, int bottomOffset, boolean needsAnimate) {
                if (fullscreenManager.getTopVisibleContentOffset() != initialVisibleContentOffset) {
                    contentMovedCallback.notifyCalled();
                    fullscreenManager.removeListener(this);
                }
            }

            @Override
            public void onToggleOverlayVideoMode(boolean enabled) {}

            @Override
            public void onContentOffsetChanged(int offset) {}

            @Override
            public void onBottomControlsHeightChanged(int bottomControlsHeight) {}
        });

        float dragX = 50f;
        float dragStartY = tab.getView().getHeight() - 50f;

        WebContents webContents = tab.getWebContents();

        final CallbackHelper scrollEndCallback = new CallbackHelper();
        final CallbackHelper flingEndCallback = new CallbackHelper();
        GestureStateListener scrollEndListener = new GestureStateListener() {
            @Override
            public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                scrollEndCallback.notifyCalled();
            }

            @Override
            public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                flingEndCallback.notifyCalled();
            }

        };
        GestureListenerManager gestureListenerManager =
                WebContentsUtils.getGestureListenerManager(webContents);
        gestureListenerManager.addListener(scrollEndListener);

        for (int i = 0; i < 10; i++) {
            int numScrollEndCalled = scrollEndCallback.getCallCount();
            int numFlingEndCalled = flingEndCallback.getCallCount();
            float dragEndY = dragStartY - fullscreenManager.getTopControlsHeight();

            long downTime = SystemClock.uptimeMillis();
            TouchCommon.dragStart(testRule.getActivity(), dragX, dragStartY, downTime);
            TouchCommon.dragTo(
                    testRule.getActivity(), dragX, dragX, dragStartY, dragEndY, 100, downTime);
            TouchCommon.dragEnd(testRule.getActivity(), dragX, dragEndY, downTime);

            try {
                contentMovedCallback.waitForCallback(0, 1, 500, TimeUnit.MILLISECONDS);

                try {
                    scrollEndCallback.waitForCallback(
                            numScrollEndCalled, 1, 5000, TimeUnit.MILLISECONDS);
                    flingEndCallback.waitForCallback(
                            numFlingEndCalled, 1, 5000, TimeUnit.MILLISECONDS);
                } catch (TimeoutException e) {
                    Assert.fail("Didn't get expected ScrollEnd gestures");
                }

                try {
                    flingEndCallback.waitForCallback(
                            numFlingEndCalled, 1, 200, TimeUnit.MILLISECONDS);
                } catch (TimeoutException e) {
                    // Depending on timing - the above scroll may not have
                    // generated a fling. If it did, it the fling end may
                    // sometimes be called after the scroll end so wait a little
                    // for it.
                }

                numFlingEndCalled = flingEndCallback.getCallCount();

                scrollBrowserControls(testRule, false);
                scrollBrowserControls(testRule, true);

                // Make sure the gesture stream is finished before we hand back control.
                try {
                    scrollEndCallback.waitForCallback(
                            numScrollEndCalled + 1, 2, 5000, TimeUnit.MILLISECONDS);
                } catch (TimeoutException e) {
                    Assert.fail("Didn't get expected ScrollEnd gestures");
                }

                try {
                    flingEndCallback.waitForCallback(
                            numFlingEndCalled, 2, 200, TimeUnit.MILLISECONDS);
                } catch (TimeoutException e) {
                    // Depending on timing - the above scrolls may not have
                    // generated flings. If they did, the fling end may sometimes
                    // be called after the scroll end so wait a little for it.
                }

                gestureListenerManager.removeListener(scrollEndListener);

                return;
            } catch (TimeoutException e) {
                // Ignore and retry
            }
        }

        Assert.fail("Visible content never moved as expected.");
    }

    /**
     * Disable any browser visibility overrides for testing.
     */
    public static void disableBrowserOverrides() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> BrowserStateBrowserControlsVisibilityDelegate.disableForTesting());
    }

    public static void fling(ChromeActivityTestRule testRule, final int vx, final int vy) {
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> {
                    testRule.getWebContents().getEventForwarder().startFling(
                            SystemClock.uptimeMillis(), vx, vy, /*synthetic_scroll*/ false,
                            /*prevent_boosting*/ false);
                });
    }
}
