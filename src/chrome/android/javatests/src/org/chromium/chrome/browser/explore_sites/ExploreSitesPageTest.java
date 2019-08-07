// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import static android.support.test.espresso.Espresso.onView;

import static org.hamcrest.Matchers.instanceOf;

import android.annotation.TargetApi;
import android.os.Build;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;

/**
 * Simple test to demonstrate use of ScreenShooter rule.
 */
// TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.EXPLORE_SITES)
public class ExploreSitesPageTest {
    // clang-format on

    ArrayList<ExploreSitesCategory> getTestingCatalog() {
        final ArrayList<ExploreSitesCategory> categoryList = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            ExploreSitesCategory category =
                    new ExploreSitesCategory(i, i, "Category #" + Integer.toString(i),
                            /* ntpShownCount = */ 1, /* interactionCount = */ 0);
            // 0th category would be filtered out. Tests that row maximums are obeyed.
            int numSites = 4 * i + 1;
            for (int j = 0; j < numSites; j++) {
                ExploreSitesSite site = new ExploreSitesSite(
                        i * 8 + j, "Site #" + Integer.toString(j), "https://example.com/", false);
                category.addSite(site);
            }
            categoryList.add(category);
        }

        return categoryList;
    }

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    private Tab mTab;
    private RecyclerView mRecyclerView;
    private ExploreSitesPage mEsp;

    @Before
    public void setUp() throws Exception {
        ExploreSitesBridge.setCatalogForTesting(getTestingCatalog());
        mActivityTestRule.startMainActivityWithURL("about:blank");

        mActivityTestRule.loadUrl(UrlConstants.EXPLORE_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        waitForEspLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof ExploreSitesPage);
        mEsp = (ExploreSitesPage) mTab.getNativePage();
        mRecyclerView = mEsp.getView().findViewById(R.id.explore_sites_category_recycler);
    }

    @After
    public void tearDown() throws Exception {
        ExploreSitesBridge.setCatalogForTesting(null);
    }

    private int getFirstVisiblePosition() {
        return ((LinearLayoutManager) mRecyclerView.getLayoutManager())
                .findFirstCompletelyVisibleItemPosition();
    }

    @Test
    @SmallTest
    @DisabledTest
    @Feature({"ExploreSites", "RenderTest"})
    public void testScrolledLayout_withBack() throws Exception {
        final int scrollPosition = 2;
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(scrollPosition));
        mRenderTestRule.render(mRecyclerView, "recycler_layout");
        Assert.assertEquals(scrollPosition, getFirstVisiblePosition());
        // TODO(https://crbug.com/938519): Remove this sleep in favor of actually waiting for the
        // scroll bar to disappear.
        SystemClock.sleep(3000);
        mActivityTestRule.loadUrl("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onBackPressed());
        mRenderTestRule.render(mRecyclerView, "recycler_layout_back");
        Assert.assertEquals(scrollPosition, getFirstVisiblePosition());
    }

    @Test
    @SmallTest
    @Feature({"ExploreSites", "RenderTest"})
    public void testInitialLayout() throws Exception {
        onView(instanceOf(RecyclerView.class)).perform(RecyclerViewActions.scrollToPosition(0));
        mRenderTestRule.render(mRecyclerView, "initial_layout");
    }

    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enabled-features=ExploreSites<FakeStudyName", "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:variation/mostLikelyTile"})
    @Feature({"ExploreSites", "RenderTest"})
    public void
    testInitialLayout_MostLikely() throws Exception {
        mRenderTestRule.render(mRecyclerView, "initial_layout");
        Assert.assertEquals(0, getFirstVisiblePosition());
    }

    @Test
    @SmallTest
    @Feature({"ExploreSites", "RenderTest"})
    public void testScrollingFromNTP() throws Exception {
        mActivityTestRule.loadUrl("about:blank");
        ExploreSitesCategory category = getTestingCatalog().get(2);
        mActivityTestRule.loadUrl(category.getUrl());
        waitForEspLoaded(mTab);
        Assert.assertTrue(mTab.getNativePage() instanceof ExploreSitesPage);
        mEsp = (ExploreSitesPage) mTab.getNativePage();
        mRecyclerView = mEsp.getView().findViewById(R.id.explore_sites_category_recycler);
        mRenderTestRule.render(mRecyclerView, "scrolled_to_category_2");
        // We expect that the first visible position is actually 1 (not 2) since the first category
        // in the catalog is not added to the adapter at all due to insufficient sites.
        Assert.assertEquals(1, getFirstVisiblePosition());
    }

    @Test
    @SmallTest
    @Feature({"ExploreSites", "RenderTest"})
    public void testFocusRetention_WithBack() throws Exception {
        Assume.assumeTrue(FeatureUtilities.isNoTouchModeEnabled());

        InstrumentationRegistry.getInstrumentation().setInTouchMode(false);

        final int defaultScrollPosition = mEsp.initialScrollPositionForTests();
        Assert.assertEquals(defaultScrollPosition, getFocusedCategoryPosition());
        Assert.assertEquals(0, getFocusedTileIndex());

        // Change the focus from default so that we can verify it stays the same after navigation.
        focusDifferentCard();

        int focusedCategory = getFocusedCategoryPosition();
        int focusedTile = getFocusedTileIndex();
        Assert.assertNotEquals(defaultScrollPosition, focusedCategory);
        Assert.assertNotEquals(0, focusedTile);

        mRenderTestRule.render(mRecyclerView, "recycler_layout_focus");

        mActivityTestRule.loadUrl("about:blank");

        navigateBackToESP();

        mRenderTestRule.render(mRecyclerView, "recycler_layout_focus_back");
        Assert.assertEquals(focusedCategory, getFocusedCategoryPosition());
        Assert.assertEquals(focusedTile, getFocusedTileIndex());
    }

    private void focusDifferentCard() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ExploreSitesCategoryCardView cardView = null;
            for (int i = 0; i < mRecyclerView.getChildCount(); i++) {
                cardView = (ExploreSitesCategoryCardView) mRecyclerView.getChildAt(i);
                if (!cardView.hasFocus()) {
                    cardView.getTileViewAt(2).requestFocus();
                    break;
                }
            }
        });
    }

    private void navigateBackToESP() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onBackPressed());
        waitForEspLoaded(mTab);
        mEsp = (ExploreSitesPage) mTab.getNativePage();
        mRecyclerView = mEsp.getView().findViewById(R.id.explore_sites_category_recycler);
    }

    private int getFocusedCategoryPosition() {
        View focusedView = mRecyclerView.getFocusedChild();
        return mRecyclerView.getLayoutManager().getPosition(focusedView);
    }

    private int getFocusedTileIndex() {
        ExploreSitesCategoryCardView focusedView =
                (ExploreSitesCategoryCardView) mRecyclerView.getFocusedChild();
        return focusedView.getFocusedTileIndex(-1);
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    public static void waitForEspLoaded(final Tab tab) {
        CriteriaHelper.pollUiThread(new Criteria("ESP never fully loaded") {
            @Override
            public boolean isSatisfied() {
                if (tab.getNativePage() instanceof ExploreSitesPage) {
                    return ((ExploreSitesPage) tab.getNativePage()).isLoadedForTests();
                } else {
                    return false;
                }
            }
        });
    }
}
