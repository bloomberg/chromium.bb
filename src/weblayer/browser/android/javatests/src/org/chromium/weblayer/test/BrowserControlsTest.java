// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Instrumentation;
import android.os.Build;
import android.os.Bundle;
import android.os.RemoteException;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.weblayer.BrowserControlsOffsetCallback;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Test for bottom-controls.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
@CommandLineFlags.Add("enable-features=ImmediatelyHideBrowserControlsForTest")
public class BrowserControlsTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private BrowserControlsHelper mBrowserControlsHelper;

    // Height of the top-view. Set in setUp().
    private int mTopViewHeight;
    // Height from the page (obtained using getVisiblePageHeight()) with the top-controls.
    private int mPageHeightWithTopView;

    /**
     * Returns the visible height of the page as determined by JS. The returned value is in CSS
     * pixels (which are most likely not the same as device pixels).
     */
    private int getVisiblePageHeight() {
        return mActivityTestRule.executeScriptAndExtractInt("window.innerHeight");
    }

    private Tab getActiveTab() {
        return mActivityTestRule.getActivity().getBrowser().getActiveTab();
    }

    private TestWebLayer getTestWebLayer() {
        return TestWebLayer.getTestWebLayer(
                mActivityTestRule.getActivity().getApplicationContext());
    }

    private void setAccessibilityEnabled(boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                getTestWebLayer().setAccessibilityEnabled(value);
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        });
    }

    private boolean canBrowserControlsScroll() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            try {
                return getTestWebLayer().canBrowserControlsScroll(getActiveTab());
            } catch (RemoteException e) {
                throw new RuntimeException(e);
            }
        });
    }

    private void createActivityWithTopView() throws Exception {
        final String url = mActivityTestRule.getTestDataURL("tall_page.html");
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);

        mBrowserControlsHelper =
                BrowserControlsHelper.createAndBlockUntilBrowserControlsInitializedInSetUp(
                        activity);

        mTopViewHeight = mBrowserControlsHelper.getTopViewHeight();
        mPageHeightWithTopView = getVisiblePageHeight();
    }

    private View createBottomView() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        View bottomView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView view = new TextView(activity);
            view.setText("BOTTOM");
            activity.getBrowser().setBottomView(view);
            return view;
        });
        mBrowserControlsHelper.waitForBrowserControlsViewToBeVisible(bottomView);
        int bottomViewHeight =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return bottomView.getHeight(); });
        mBrowserControlsHelper.waitForBrowserControlsMetadataState(
                mTopViewHeight, bottomViewHeight);
        return bottomView;
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testTopAndBottom() throws Exception {
        createActivityWithTopView();
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        View bottomView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView view = new TextView(activity);
            view.setText("BOTTOM");
            activity.getBrowser().setBottomView(view);
            return view;
        });

        mBrowserControlsHelper.waitForBrowserControlsViewToBeVisible(bottomView);

        int bottomViewHeight =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return bottomView.getHeight(); });
        Assert.assertTrue(bottomViewHeight > 0);
        // The amount necessary to scroll is the sum of the two views. This is because the page
        // height is reduced by the sum of these two.
        int maxViewsHeight = mTopViewHeight + bottomViewHeight;

        // Wait for cc to see the bottom height. This is very important, as scrolling is gated by
        // cc getting the bottom height.
        mBrowserControlsHelper.waitForBrowserControlsMetadataState(
                mTopViewHeight, bottomViewHeight);

        // Adding a bottom view should change the page height.
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.not(mPageHeightWithTopView));
        });
        int pageHeightWithTopAndBottomViews = getVisiblePageHeight();
        Assert.assertTrue(pageHeightWithTopAndBottomViews < mPageHeightWithTopView);

        // Move by the size of the controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -maxViewsHeight);

        // Moving should hide the bottom View.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(bottomView.getVisibility(), Matchers.is(View.INVISIBLE));
        });
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    getVisiblePageHeight(), Matchers.greaterThan(mPageHeightWithTopView));
        });

        // Move so top and bottom-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, maxViewsHeight);

        mBrowserControlsHelper.waitForBrowserControlsViewToBeVisible(bottomView);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    getVisiblePageHeight(), Matchers.is(pageHeightWithTopAndBottomViews));
        });
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testBottomOnly() throws Exception {
        createActivityWithTopView();
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        // Remove the top-view.
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });

        // Wait for cc to see the top-controls height change.
        mBrowserControlsHelper.waitForBrowserControlsMetadataState(0, 0);

        int pageHeightWithNoTopView = getVisiblePageHeight();
        Assert.assertNotEquals(pageHeightWithNoTopView, mPageHeightWithTopView);

        // Add in the bottom-view.
        View bottomView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView view = new TextView(activity);
            view.setText("BOTTOM");
            activity.getBrowser().setBottomView(view);
            return view;
        });

        mBrowserControlsHelper.waitForBrowserControlsViewToBeVisible(bottomView);
        int bottomViewHeight =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return bottomView.getHeight(); });
        Assert.assertTrue(bottomViewHeight > 0);
        // Wait for cc to see the bottom-controls height change.
        mBrowserControlsHelper.waitForBrowserControlsMetadataState(0, bottomViewHeight);

        int pageHeightWithBottomView = getVisiblePageHeight();
        Assert.assertNotEquals(pageHeightWithNoTopView, pageHeightWithBottomView);

        // Move by the size of the bottom-controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -bottomViewHeight);

        // Moving should hide the bottom-controls View.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(bottomView.getVisibility(), Matchers.is(View.INVISIBLE));
        });
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.is(pageHeightWithNoTopView));
        });

        // Move so bottom-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, bottomViewHeight);

        mBrowserControlsHelper.waitForBrowserControlsViewToBeVisible(bottomView);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.is(pageHeightWithBottomView));
        });
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testTopOnly() throws Exception {
        createActivityWithTopView();
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        View topView = activity.getTopContentsContainer();

        // Move by the size of the top-controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mTopViewHeight);

        // Moving should hide the top-controls and change the page height.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(topView.getVisibility(), Matchers.is(View.INVISIBLE)));
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.not(mPageHeightWithTopView));
        });

        // Move so top-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, mTopViewHeight);

        // Wait for the page height to match initial height.
        mBrowserControlsHelper.waitForBrowserControlsViewToBeVisible(topView);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.is(mPageHeightWithTopView));
        });
    }

    @MinWebLayerVersion(94)
    @Test
    @SmallTest
    public void testEvents() throws Exception {
        createActivityWithTopView();
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        View topView = activity.getTopContentsContainer();
        View bottomView = createBottomView();

        int fragmentHeight = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return mActivityTestRule.getFragment().getView().getHeight(); });
        int pageHeight = getVisiblePageHeight();

        // Record the maximum y position of clicks to detect if we clicked at the top or bottom.
        mActivityTestRule.executeScriptSync(
                "var max = 0; document.onclick = (e) => { max = Math.max(max, e.pageY); };",
                true /* useSeparateIsolate */);

        // Clicks on the bottom view should not propagate to the content layer.
        TestTouchUtils.singleClickView(instrumentation, bottomView, 0, 0);
        TestTouchUtils.sleepForDoubleTapTimeout(instrumentation);

        // Click below the top view inside the content layer in the middle of the page.
        TestTouchUtils.singleClickView(
                instrumentation, topView, 0, topView.getHeight() + fragmentHeight / 2);
        TestTouchUtils.sleepForDoubleTapTimeout(instrumentation);

        // Wait until we see the click from the middle of the page.
        CriteriaHelper.pollInstrumentationThread(() -> {
            int max = mActivityTestRule.executeScriptAndExtractInt("max");
            Criteria.checkThat(max, Matchers.greaterThan(pageHeight * 1 / 4));
            Criteria.checkThat(max, Matchers.lessThan(pageHeight * 3 / 4));
        });
    }

    @MinWebLayerVersion(100)
    @Test
    @SmallTest
    public void testEventBelowView() throws Exception {
        createActivityWithTopView();
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        View topContents = activity.getTopContentsContainer();

        View fragmentView = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return mActivityTestRule.getFragment().getView(); });

        // Move by the size of the top-controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mTopViewHeight);

        // Moving should collapse the top-controls to their min height and change the page height.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(topContents.getVisibility(), Matchers.is(View.INVISIBLE));
        });
        // Click to stop any flings.
        TestTouchUtils.singleClickView(instrumentation, activity.getWindow().getDecorView());
        TestTouchUtils.sleepForDoubleTapTimeout(instrumentation);

        // Record when page is clicked.
        mActivityTestRule.executeScriptSync(
                "var didClick = false; document.onclick = (e) => { didClick = true; };",
                true /* useSeparateIsolate */);

        // Click the area inside where top control would be if it's not hidden.
        TestTouchUtils.singleClickView(instrumentation, fragmentView, 0, mTopViewHeight / 2);
        TestTouchUtils.sleepForDoubleTapTimeout(instrumentation);

        // Wait until page sees click.
        CriteriaHelper.pollInstrumentationThread(() -> {
            boolean didClick = mActivityTestRule.executeScriptAndExtractBoolean("didClick");
            Criteria.checkThat(didClick, Matchers.is(true));
        });
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testTopMinHeight() throws Exception {
        createActivityWithTopView();
        final int minHeight = 20;
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        View topContents = activity.getTopContentsContainer();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getBrowser().setTopView(topContents, minHeight, false, false));
        int expectedCollapseAmount = topContents.getHeight() - minHeight;

        // Make sure the top controls start out taller than the min height.
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(topContents.getHeight(), Matchers.greaterThan(minHeight));
        });

        // Move by the size of the top-controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mTopViewHeight);

        // Moving should collapse the top-controls to their min height and change the page height.
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    getVisiblePageHeight(), Matchers.greaterThan(mPageHeightWithTopView));
            Criteria.checkThat(
                    topContents.getTranslationY(), Matchers.is((float) -expectedCollapseAmount));
        });

        // Move so top-controls are shown again.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, mTopViewHeight);

        // Wait for the page height to match initial height.
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.is(mPageHeightWithTopView));
        });
    }

    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @DisabledTest(message = "https://crbug.com/1256861")
    @Test
    @SmallTest
    public void testOnlyExpandTopControlsAtPageTop() throws Exception {
        createActivityWithTopView();
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        View topContents = activity.getTopContentsContainer();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> activity.getBrowser().setTopView(
                                topContents, 0, /*onlyExpandControlsAtPageTop=*/true, false));

        // Scroll down past the top-controls, which should collapse the top-controls and change the
        // page height.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -2 * mTopViewHeight);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(
                    getVisiblePageHeight(), Matchers.greaterThan(mPageHeightWithTopView));
            Criteria.checkThat(activity.getTopContentsContainer().getVisibility(),
                    Matchers.is(View.INVISIBLE));
        });

        // Scroll part of the way up again, which should not show the top controls.
        int scrolledPageHeight = getVisiblePageHeight();
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, mTopViewHeight);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.is(scrolledPageHeight));
        });

        // Scroll to the top to show the top controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, 2 * mTopViewHeight);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.is(mPageHeightWithTopView));
            Criteria.checkThat(
                    activity.getTopContentsContainer().getVisibility(), Matchers.is(View.VISIBLE));
            Criteria.checkThat(topContents.getTranslationY(), Matchers.is(0.f));
        });
    }

    // Makes sure that the top controls are not shown when a js dialog is shown when
    // onlyExpandTopControlsAtPageTop is true and the page is scrolled down.
    //
    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @DisabledTest(message = "https://crbug.com/1319870")
    @Test
    @SmallTest
    public void testAlertDoesntShowTopControlsIfOnlyExpandTopControlsAtPageTop() throws Exception {
        createActivityWithTopView();
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        View topContents = activity.getTopContentsContainer();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> activity.getBrowser().setTopView(
                                topContents, 0, /*onlyExpandControlsAtPageTop=*/true, false));

        // Scroll past the top-controls and wait until they're not visible.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -2 * mTopViewHeight);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(activity.getTopContentsContainer().getVisibility(),
                    Matchers.is(View.INVISIBLE));
        });

        // Trigger an alert dialog.
        mActivityTestRule.executeScriptSync(
                "window.setTimeout(function() { alert('alert dialog'); }, 1);", false);
        onView(withText("alert dialog")).check(matches(isDisplayed()));

        // Top controls shouldn't be shown.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(activity.getTopContentsContainer().getVisibility(),
                    Matchers.is(View.INVISIBLE));
        });
    }

    /**
     * Makes sure that the top controls are shown when a js dialog is shown.
     *
     * Regression test for https://crbug.com/1078181.
     *
     * Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
     */
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testAlertShowsTopControls() throws Exception {
        createActivityWithTopView();
        InstrumentationActivity activity = mActivityTestRule.getActivity();

        // Move by the size of the top-controls.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mTopViewHeight);

        // Wait till top controls are invisible.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(activity.getTopContentsContainer().getVisibility(),
                    Matchers.is(View.INVISIBLE));
        });

        // Trigger an alert dialog.
        mActivityTestRule.executeScriptSync(
                "window.setTimeout(function() { alert('alert'); }, 1);", false);

        // Top controls are shown.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    activity.getTopContentsContainer().getVisibility(), Matchers.is(View.VISIBLE));
        });
    }

    // Tests various assertions when accessibility is enabled.
    // Disabled on L bots due to unexplained flakes. See crbug.com/1035894.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testAccessibility() throws Exception {
        createActivityWithTopView();
        InstrumentationActivity activity = mActivityTestRule.getActivity();

        // Scroll such that top-controls are hidden.
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mTopViewHeight);
        View topView = activity.getTopContentsContainer();
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(topView.getVisibility(), Matchers.is(View.INVISIBLE)));
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.not(mPageHeightWithTopView));
        });

        // Turn on accessibility, which should force the controls to show.
        setAccessibilityEnabled(true);
        mBrowserControlsHelper.waitForBrowserControlsViewToBeVisible(
                activity.getTopContentsContainer());
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.is(mPageHeightWithTopView));
        });

        // When accessibility is enabled, the controls are not allowed to scroll.
        Assert.assertFalse(canBrowserControlsScroll());

        // Turn accessibility off, and verify the controls can scroll. This polls as
        // setAccessibilityEnabled() is async.
        setAccessibilityEnabled(false);
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(canBrowserControlsScroll(), Matchers.is(true)));
    }

    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testRemoveAllFromTopView() throws Exception {
        createActivityWithTopView();
        InstrumentationActivity activity = mActivityTestRule.getActivity();

        // Install a different top-view.
        FrameLayout newTopView = TestThreadUtils.runOnUiThreadBlocking(() -> {
            FrameLayout frameLayout = new FrameLayout(activity);
            TextView textView = new TextView(activity);
            textView.setText("new top");
            frameLayout.addView(textView,
                    new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            FrameLayout.LayoutParams.UNSPECIFIED_GRAVITY));
            activity.getBrowser().setTopView(frameLayout);
            return frameLayout;
        });

        // Wait till new view is visible.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(newTopView.getVisibility(), Matchers.is(View.VISIBLE)));
        int newTopViewHeight =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return newTopView.getHeight(); });
        Assert.assertNotEquals(newTopViewHeight, mTopViewHeight);
        Assert.assertTrue(newTopViewHeight > 0);
        mBrowserControlsHelper.waitForBrowserControlsMetadataState(newTopViewHeight, 0);

        // Remove all, and ensure metadata and page-height change.
        TestThreadUtils.runOnUiThreadBlocking(() -> { newTopView.removeAllViews(); });
        mBrowserControlsHelper.waitForBrowserControlsMetadataState(0, 0);
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(getVisiblePageHeight(), Matchers.not(mPageHeightWithTopView));
        });
    }

    private void registerBrowserControlsOffsetCallbackForOffset(
            CallbackHelper helper, int targetOffset) {
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowser().registerBrowserControlsOffsetCallback(
                    new BrowserControlsOffsetCallback() {
                        @Override
                        public void onTopViewOffsetChanged(int offset) {
                            if (offset == targetOffset) {
                                activity.getBrowser().unregisterBrowserControlsOffsetCallback(this);
                                helper.notifyCalled();
                            }
                        }
                    });
        });
    }

    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testTopExpandedWhenOnlyExpandAtTop() throws Exception {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_ONLY_EXPAND_CONTROLS_AT_TOP, true);
        final String url = mActivityTestRule.getTestDataURL("tall_page.html");
        InstrumentationActivity activity = mActivityTestRule.launchShell(extras);
        CallbackHelper helper = new CallbackHelper();
        registerBrowserControlsOffsetCallbackForOffset(helper, 0);
        mActivityTestRule.navigateAndWait(url);
        helper.waitForFirst();
    }

    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Test
    @SmallTest
    public void testTopExpandedOnLoadWhenOnlyExpandAtTop() throws Exception {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_ONLY_EXPAND_CONTROLS_AT_TOP, true);
        InstrumentationActivity activity = mActivityTestRule.launchShell(extras);
        CallbackHelper helper = new CallbackHelper();
        registerBrowserControlsOffsetCallbackForOffset(helper, 0);
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("tall_page.html"));
        int callCount = 0;
        helper.waitForCallback(callCount++);

        mTopViewHeight = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return activity.getTopContentsContainer().getHeight(); });
        Assert.assertNotEquals(mTopViewHeight, 0);

        // Scroll such that top-controls are hidden.
        registerBrowserControlsOffsetCallbackForOffset(helper, -mTopViewHeight);
        EventUtils.simulateDragFromCenterOfView(
                activity.getWindow().getDecorView(), 0, -mTopViewHeight);
        helper.waitForCallback(callCount++);

        // Load a new page. The top-controls should be shown again.
        registerBrowserControlsOffsetCallbackForOffset(helper, 0);
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("simple_page.html"));
        helper.waitForCallback(callCount++);
    }
}
