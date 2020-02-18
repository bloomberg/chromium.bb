// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.FEED_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_BOTTOM_BAR_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.feed.FeedSurfaceCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.UiRestriction;

/** Tests for {@link ExploreSurfaceViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ExploreSurfaceViewBinderTest {
    private ExploreSurfaceCoordinator mExploreSurfaceCoordinator;
    private FeedSurfaceCoordinator mFeedSurfaceCoordinator;
    private View mFeedSurfaceView;
    private PropertyModel mPropertyModel;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityFromLauncher();

        // TODO(crbug.com/1025296): Investigate to use DummyUiActivityTestCase. We can not do that
        // since mocked FeedSurfaceCoordinator does not work as expected in release build (works
        // well in debug build).
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel = new PropertyModel(StartSurfaceProperties.ALL_KEYS);
            mExploreSurfaceCoordinator =
                    new ExploreSurfaceCoordinator(mActivityTestRule.getActivity(),
                            mActivityTestRule.getActivity().getCompositorViewHolder(),
                            mPropertyModel, true);
            mFeedSurfaceCoordinator =
                    mExploreSurfaceCoordinator.getFeedSurfaceCreator().createFeedSurfaceCoordinator(
                            false);
            mFeedSurfaceView = mFeedSurfaceCoordinator.getView();
        });
    }

    @Test
    @SmallTest
    public void testSetVisibilityWithoutFeedSurfaceCoordinator() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        });
        assertEquals(mFeedSurfaceView.getParent(), null);
    }

    @Test
    @SmallTest
    public void testSetVisibilityWithFeedSurfaceCoordinator() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(FEED_SURFACE_COORDINATOR, mFeedSurfaceCoordinator);
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        });
        assertNotEquals(mFeedSurfaceView.getParent(), null);
        assertEquals(mFeedSurfaceView.getVisibility(), View.VISIBLE);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, false); });
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPropertyModel.set(IS_SHOWING_OVERVIEW, false); });
        assertEquals(mFeedSurfaceView.getParent(), null);
    }

    @Test
    @SmallTest
    public void testSetVisibilityWithBottomBarVisible() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(FEED_SURFACE_COORDINATOR, mFeedSurfaceCoordinator);
            mPropertyModel.set(IS_BOTTOM_BAR_VISIBLE, true);
            mPropertyModel.set(BOTTOM_BAR_HEIGHT, 10);
            mPropertyModel.set(TOP_BAR_HEIGHT, 20);
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        });

        assertNotEquals(mFeedSurfaceView.getParent(), null);
        assertEquals(mFeedSurfaceView.getVisibility(), View.VISIBLE);
        FrameLayout.LayoutParams layoutParams =
                (FrameLayout.LayoutParams) mFeedSurfaceView.getLayoutParams();
        assertEquals(layoutParams.bottomMargin, 10);
        assertEquals(layoutParams.topMargin, 20);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, false); });
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPropertyModel.set(IS_SHOWING_OVERVIEW, false); });
        assertEquals(mFeedSurfaceView.getParent(), null);
    }

    @Test
    @SmallTest
    public void testSetVisibilityAfterShowingOverview() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(FEED_SURFACE_COORDINATOR, mFeedSurfaceCoordinator);
            mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        });
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true); });
        assertNotEquals(mFeedSurfaceView.getParent(), null);
        assertEquals(mFeedSurfaceView.getVisibility(), View.VISIBLE);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, false); });
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPropertyModel.set(IS_SHOWING_OVERVIEW, false); });
        assertEquals(mFeedSurfaceView.getParent(), null);
    }

    @Test
    @SmallTest
    public void testSetVisibilityBeforeShowingOverview() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertFalse(mPropertyModel.get(IS_EXPLORE_SURFACE_VISIBLE));
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPropertyModel.set(FEED_SURFACE_COORDINATOR, mFeedSurfaceCoordinator);
            mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, true);
        });
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPropertyModel.set(IS_SHOWING_OVERVIEW, true); });
        assertNotEquals(mFeedSurfaceView.getParent(), null);
        assertEquals(mFeedSurfaceView.getVisibility(), View.VISIBLE);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPropertyModel.set(IS_SHOWING_OVERVIEW, false); });
        assertEquals(mFeedSurfaceView.getParent(), null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPropertyModel.set(IS_EXPLORE_SURFACE_VISIBLE, false); });
        assertEquals(mFeedSurfaceView.getParent(), null);
    }
}
