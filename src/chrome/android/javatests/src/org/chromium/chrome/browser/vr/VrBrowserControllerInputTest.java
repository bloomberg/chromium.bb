// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.graphics.PointF;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryPage;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.WebContentsUtils;
import org.chromium.content_public.browser.RenderCoordinates;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * End-to-end tests for Daydream controller input while in the VR browser.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
public class VrBrowserControllerInputTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    private VrBrowserTestFramework mVrBrowserTestFramework;
    private EmulatedVrController mController;

    @Before
    public void setUp() throws Exception {
        // Ensure that all frame updates are delivered to the browser so we can monitor for
        // scroll changes.
        WebContentsUtils.reportAllFrameSubmissions(mVrTestRule.getWebContents(), true);

        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        mController = new EmulatedVrController(mVrTestRule.getActivity());
        mController.recenterView();
    }

    private void waitForPageToBeScrollable(final RenderCoordinates coord) {
        final View view = mVrTestRule.getActivity().getActivityTab().getContentView();
        CriteriaHelper.pollUiThread(() -> {
            return coord.getContentHeightPixInt() > view.getHeight()
                    && coord.getContentWidthPixInt() > view.getWidth();
        }, "Page did not become scrollable", POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_LONG_MS);
    }

    /**
     * Verifies that swiping up/down/left/right on the Daydream controller's
     * touchpad scrolls the webpage while in the VR browser.
     */
    @Test
    @MediumTest
    public void testControllerScrolling() throws InterruptedException, Exception {
        String url = VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_controller_scrolling");

        final AtomicReference<RenderCoordinates> coord = new AtomicReference<RenderCoordinates>();
        Runnable waitScrollable = () -> {
            coord.set(RenderCoordinates.fromWebContents(mVrTestRule.getWebContents()));
            waitForPageToBeScrollable(coord.get());
        };

        Callable<Integer> getYCoord = () -> {
            return coord.get().getScrollYPixInt();
        };
        Callable<Integer> getXCoord = () -> {
            return coord.get().getScrollXPixInt();
        };

        testControllerScrollingImpl(url, waitScrollable, getYCoord, getXCoord);
    }

    /**
     * Verifies that scrolling via the Daydream controller's touchpad works in cross-origin iframes
     * (file:// URLs appear to be always treated as different origins).
     * Automation of a manual test in https://crbug.com/862153.
     */
    @Test
    @MediumTest
    public void testControllerScrollingIframe() throws InterruptedException, Exception {
        String url = VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                "test_controller_scrolling_iframe_outer");

        Runnable waitScrollable = () -> {
            // We need to focus the iframe before we can start running JavaScript in it.
            mVrBrowserTestFramework.runJavaScriptOrFail(
                    "document.getElementById('fs_iframe').focus()", POLL_TIMEOUT_SHORT_MS);
            mVrBrowserTestFramework.pollJavaScriptBooleanInFrameOrFail(
                    "document.documentElement.scrollHeight > document.documentElement.clientHeight",
                    POLL_TIMEOUT_LONG_MS);
        };

        Callable<Integer> getYCoord = () -> {
            // Round necessary to prevent Integer from failing due to decimal points.
            return Integer.valueOf(mVrBrowserTestFramework.runJavaScriptInFrameOrFail(
                    "Math.round(document.documentElement.scrollTop)", POLL_TIMEOUT_SHORT_MS));
        };
        Callable<Integer> getXCoord = () -> {
            // Round necessary to prevent Integer from failing due to decimal points.
            return Integer.valueOf(mVrBrowserTestFramework.runJavaScriptInFrameOrFail(
                    "Math.round(document.documentElement.scrollLeft)", POLL_TIMEOUT_SHORT_MS));
        };

        testControllerScrollingImpl(url, waitScrollable, getYCoord, getXCoord);
    }

    private void testControllerScrollingImpl(String url, Runnable waitScrollable,
            Callable<Integer> getYCoord, Callable<Integer> getXCoord)
            throws InterruptedException, Exception {
        mVrTestRule.loadUrl(url, PAGE_LOAD_TIMEOUT_S);
        waitScrollable.run();

        // Test that scrolling down works.
        int startScrollPoint = getYCoord.call().intValue();
        // Arbitrary, but valid values to scroll smoothly.
        int scrollSteps = 20;
        int scrollSpeed = 60;
        mController.scroll(EmulatedVrController.ScrollDirection.DOWN, scrollSteps, scrollSpeed);
        // We need this second scroll down, otherwise the horizontal scrolling becomes flaky
        // This actually seems to not be an issue in this test case anymore, but still occurs in
        // the fling scroll test, so keep around here as an extra precaution.
        // TODO(bsheedy): Figure out why this is the case.
        mController.scroll(EmulatedVrController.ScrollDirection.DOWN, scrollSteps, scrollSpeed);
        int endScrollPoint = getYCoord.call().intValue();
        Assert.assertTrue("Controller failed to scroll down", startScrollPoint < endScrollPoint);

        // Test that scrolling up works.
        startScrollPoint = endScrollPoint;
        mController.scroll(EmulatedVrController.ScrollDirection.UP, scrollSteps, scrollSpeed);
        endScrollPoint = getYCoord.call().intValue();
        Assert.assertTrue("Controller failed to scroll up", startScrollPoint > endScrollPoint);

        // Test that scrolling right works.
        startScrollPoint = getXCoord.call().intValue();
        mController.scroll(EmulatedVrController.ScrollDirection.RIGHT, scrollSteps, scrollSpeed);
        endScrollPoint = getXCoord.call().intValue();
        Assert.assertTrue("Controller failed to scroll right", startScrollPoint < endScrollPoint);

        // Test that scrolling left works.
        startScrollPoint = endScrollPoint;
        mController.scroll(EmulatedVrController.ScrollDirection.LEFT, scrollSteps, scrollSpeed);
        endScrollPoint = getXCoord.call().intValue();
        Assert.assertTrue("Controller failed to scroll left", startScrollPoint > endScrollPoint);
    }

    /**
     * Verifies that fling scrolling works on the Daydream controller's touchpad.
     */
    @Test
    @MediumTest
    public void testControllerFlingScrolling() throws InterruptedException {
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_controller_scrolling"),
                PAGE_LOAD_TIMEOUT_S);
        final RenderCoordinates coord =
                RenderCoordinates.fromWebContents(mVrTestRule.getWebContents());
        waitForPageToBeScrollable(coord);

        // Arbitrary, but valid values to trigger fling scrolling.
        int scrollSteps = 10;
        int scrollSpeed = 10;

        // Test fling scrolling down.
        mController.scroll(EmulatedVrController.ScrollDirection.DOWN, scrollSteps, scrollSpeed);
        final AtomicInteger endScrollPoint = new AtomicInteger(coord.getScrollYPixInt());
        // Check that we continue to scroll past wherever we were when we let go of the touchpad.
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> { return coord.getScrollYPixInt() > endScrollPoint.get(); },
                "Controller failed to fling scroll down", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_LONG_MS);
        mController.cancelFlingScroll();

        // Test fling scrolling up.
        mController.scroll(EmulatedVrController.ScrollDirection.UP, scrollSteps, scrollSpeed);
        endScrollPoint.set(coord.getScrollYPixInt());
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> { return coord.getScrollYPixInt() < endScrollPoint.get(); },
                "Controller failed  to fling scroll up", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_LONG_MS);
        mController.cancelFlingScroll();
        // Horizontal scrolling becomes flaky if the scroll bar is at the top when we try to scroll
        // horizontally, so scroll down a bit to ensure that isn't the case.
        mController.scroll(EmulatedVrController.ScrollDirection.DOWN, 10, 60);

        // Test fling scrolling right.
        mController.scroll(EmulatedVrController.ScrollDirection.RIGHT, scrollSteps, scrollSpeed);
        endScrollPoint.set(coord.getScrollXPixInt());
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> { return coord.getScrollXPixInt() > endScrollPoint.get(); },
                "Controller failed to fling scroll right", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_LONG_MS);
        mController.cancelFlingScroll();

        // Test fling scrolling left.
        mController.scroll(EmulatedVrController.ScrollDirection.LEFT, scrollSteps, scrollSpeed);
        endScrollPoint.set(coord.getScrollXPixInt());
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> { return coord.getScrollXPixInt() < endScrollPoint.get(); },
                "Controller failed to fling scroll left", POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_LONG_MS);
    }

    /**
     * Verifies that controller clicks in the VR browser are properly registered on the webpage.
     * This is done by clicking on a link on the page and ensuring that it causes a navigation.
     */
    @Test
    @MediumTest
    public void testControllerClicksRegisterOnWebpage() throws InterruptedException {
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                                    "test_controller_clicks_register_on_webpage"),
                PAGE_LOAD_TIMEOUT_S);

        mController.performControllerClick();
        ChromeTabUtils.waitForTabPageLoaded(mVrTestRule.getActivity().getActivityTab(),
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page"));
    }

    /**
     * Verifies that controller clicks in the VR browser on cross-origin iframes are properly
     * registered. This is done by clicking on a link in the iframe and ensuring that it causes a
     * navigation.
     * Automation of a manual test in https://crbug.com/862153.
     */
    @Test
    @MediumTest
    public void testControllerClicksRegisterOnIframe() throws InterruptedException {
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_iframe_clicks_outer"));
        mController.performControllerClick();
        // Wait until the iframe's current location matches the URL of the page that gets navigated
        // to on click.
        mVrBrowserTestFramework.pollJavaScriptBooleanInFrameOrFail("window.location.href == '"
                        + VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                                  "test_iframe_clicks_inner_nav")
                        + "'",
                POLL_TIMEOUT_SHORT_MS);
    }

    /*
     * Verifies that swiping up/down on the Daydream controller's touchpad
     * scrolls a native page while in the VR browser.
     */
    @Test
    @MediumTest
    public void testControllerScrollingNative() throws InterruptedException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        // Fill history with enough items to scroll
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_controller_scrolling"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile("generic_webvr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_webvr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_webvr_autopresent"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_gamepad_button"),
                PAGE_LOAD_TIMEOUT_S);

        mVrTestRule.loadUrl("chrome://history", PAGE_LOAD_TIMEOUT_S);

        RecyclerView recyclerView =
                ((HistoryPage) (mVrTestRule.getActivity().getActivityTab().getNativePage()))
                        .getHistoryManagerForTesting()
                        .getRecyclerViewForTests();

        // Test that scrolling down works
        int startScrollPoint = recyclerView.computeVerticalScrollOffset();
        // Arbitrary, but valid values to scroll smoothly
        int scrollSteps = 20;
        int scrollSpeed = 60;
        mController.scroll(EmulatedVrController.ScrollDirection.DOWN, scrollSteps, scrollSpeed);
        int endScrollPoint = recyclerView.computeVerticalScrollOffset();
        Assert.assertTrue("Controller failed to scroll down", startScrollPoint < endScrollPoint);

        // Test that scrolling up works
        startScrollPoint = endScrollPoint;
        mController.scroll(EmulatedVrController.ScrollDirection.UP, scrollSteps, scrollSpeed);
        endScrollPoint = recyclerView.computeVerticalScrollOffset();
        Assert.assertTrue("Controller failed to scroll up", startScrollPoint > endScrollPoint);
    }

    /**
     * Verifies that pressing the Daydream controller's 'app' button causes the user to exit
     * fullscreen
     */
    @Test
    @MediumTest
    public void testAppButtonExitsFullscreen() throws InterruptedException, TimeoutException {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page"),
                PAGE_LOAD_TIMEOUT_S);
        // Enter fullscreen
        DOMUtils.clickNode(mVrBrowserTestFramework.getFirstTabWebContents(), "fullscreen",
                false /* goThroughRootAndroidView */);
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        Assert.assertTrue("Page did not enter fullscreen",
                DOMUtils.isFullscreen(mVrBrowserTestFramework.getFirstTabWebContents()));

        mController.pressReleaseAppButton();
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    try {
                        return !DOMUtils.isFullscreen(
                                mVrBrowserTestFramework.getFirstTabWebContents());
                    } catch (InterruptedException | TimeoutException e) {
                        return false;
                    }
                },
                "Page did not exit fullscreen after app button was pressed", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_LONG_MS);
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Verifies that clicking and dragging down while at the top of the page triggers a page
     * refresh. Automation of a manual test case from https://crbug.com/861949.
     */
    @Test
    @MediumTest
    public void testDragRefresh() throws InterruptedException, TimeoutException {
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_controller_scrolling"),
                PAGE_LOAD_TIMEOUT_S);
        waitForPageToBeScrollable(RenderCoordinates.fromWebContents(mVrTestRule.getWebContents()));
        // The navigationStart time should change anytime we refresh, so save the value and compare
        // later.
        // Use a double because apparently returning Unix timestamps as floating point is a logical
        // thing for JavaScript to do and Long.valueOf is afraid of decimal points.
        String navStart = "window.performance.timing.navigationStart";
        final double navigationTimestamp =
                Double.valueOf(mVrBrowserTestFramework.runJavaScriptOrFail(
                                       navStart, POLL_TIMEOUT_SHORT_MS))
                        .doubleValue();

        // Click and drag from near the top center of the page to near the top bottom.
        // Use the NativeUiUtils approach to controller input since we shouldn't be missing anything
        // by bypassing VrCore for this test.
        NativeUiUtils.clickAndDragElement(UserFriendlyElementName.CONTENT_QUAD, new PointF(0, 0.4f),
                new PointF(0, -0.4f), 10 /* numInterpolatedSteps */);

        // Wait for the navigationStart time to be newer than our saved time.
        CriteriaHelper.pollInstrumentationThread(() -> {
            return Double.valueOf(mVrBrowserTestFramework.runJavaScriptOrFail(
                                          navStart, POLL_TIMEOUT_SHORT_MS))
                           .doubleValue()
                    > navigationTimestamp;
        });
    }
}
