// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_BASE_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.sClassParamsForStartSurfaceTest;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.HomeButton;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.List;

/**
 * Integration tests of the tab switcher when {@link StartSurface} is enabled.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class StartSurfaceTabSwitcherTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = sClassParamsForStartSurfaceTest;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    /**
     * Whether feature {@link ChromeFeatureList#INSTANT_START} is enabled.
     */
    private final boolean mUseInstantStart;

    /**
     * Whether feature {@link ChromeFeatureList#TAB_SWITCHER_ON_RETURN} is enabled as "immediately".
     * When immediate return is enabled, the Start surface is showing when Chrome is launched.
     */
    private final boolean mImmediateReturn;

    private CallbackHelper mLayoutChangedCallbackHelper;
    private LayoutStateProvider.LayoutStateObserver mLayoutObserver;
    @LayoutType
    private int mCurrentlyActiveLayout;
    public StartSurfaceTabSwitcherTest(boolean useInstantStart, boolean immediateReturn) {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.INSTANT_START, useInstantStart);

        mUseInstantStart = useInstantStart;
        mImmediateReturn = immediateReturn;
    }

    @Before
    public void setUp() throws IOException {
        StartSurfaceTestUtils.setUpStartSurfaceTests(mImmediateReturn, mActivityTestRule);

        mLayoutChangedCallbackHelper = new CallbackHelper();

        if (isInstantReturn()) {
            // Assume start surface is shown immediately, and the LayoutStateObserver may miss the
            // first onFinishedShowing event.
            mCurrentlyActiveLayout = LayoutType.TAB_SWITCHER;
        }

        mLayoutObserver = new LayoutStateProvider.LayoutStateObserver() {
            @Override
            public void onFinishedShowing(@LayoutType int layoutType) {
                mCurrentlyActiveLayout = layoutType;
                mLayoutChangedCallbackHelper.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getLayoutManagerSupplier().addObserver((manager) -> {
                if (manager.getActiveLayout() != null) {
                    mCurrentlyActiveLayout = manager.getActiveLayout().getLayoutType();
                    mLayoutChangedCallbackHelper.notifyCalled();
                }
                manager.addObserver(mLayoutObserver);
            });
        });
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testShow_SingleAsTabSwitcher() {
        if (mImmediateReturn) {
            StartSurfaceTestUtils.waitForOverviewVisible(
                    mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
            StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());
            if (isInstantReturn()) {
                // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
                // omnibox.
                return;
            }
            // Single surface is shown as homepage. Clicks "more_tabs" button to get into tab
            // switcher.
            StartSurfaceTestUtils.clickMoreTabs(mActivityTestRule.getActivity());
        } else {
            TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        }

        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view));

        OverviewModeBehaviorWatcher hideWatcher =
                TabUiTestHelper.createOverviewHideWatcher(mActivityTestRule.getActivity());
        onViewWaiting(allOf(withParent(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body)),
                              withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        hideWatcher.waitForBehavior();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testShow_SingleAsHomepage_CloseAllTabsShouldHideTabSwitcher() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        assertEquals(cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_switcher_title)
                             .getVisibility(),
                View.VISIBLE);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.getTabModelSelector().getModel(false).closeAllTabs(); });
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);
        assertEquals(cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_switcher_title)
                             .getVisibility(),
                View.GONE);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface", "TabGroup"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @FlakyTest(message = "https://crbug.com/1232695")
    public void testCreateTabWithinTabGroup() throws Exception {
        // Create tab state files for a group with two tabs.
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1});

        // Restart and open tab grid dialog.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                           instanceof TabGroupModelFilter);
        TabGroupModelFilter filter = (TabGroupModelFilter) cta.getTabModelSelector()
                                             .getTabModelFilterProvider()
                                             .getTabModelFilter(false);
        if (mImmediateReturn) {
            StartSurfaceTestUtils.clickFirstTabInCarousel();
        } else {
            onViewWaiting(allOf(withId(org.chromium.chrome.tab_ui.R.id.toolbar_left_button),
                                  isDescendantOfA(withId(
                                          org.chromium.chrome.start_surface.R.id.bottom_controls))))
                    .perform(click());
        }
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                        withParent(withId(org.chromium.chrome.tab_ui.R.id.dialog_container_view))))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(2));

        // Show start surface through tab grid dialog toolbar plus button and create a new tab by
        // clicking on MV tiles.
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.toolbar_right_button),
                       isDescendantOfA(
                               withId(org.chromium.chrome.tab_ui.R.id.dialog_container_view))))
                .perform(click());
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 2);

        // Verify a tab is created within the group by checking the tab strip and tab model.
        onView(withId(org.chromium.chrome.tab_ui.R.id.toolbar_container_view))
                .check(waitForView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                        isCompletelyDisplayed())));
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                       withParent(withId(org.chromium.chrome.tab_ui.R.id.toolbar_container_view))))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(3));
        assertEquals(1, filter.getTabGroupCount());

        // Show start surface through tab strip plus button and create a new tab by perform a query
        // search in fake box.
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.toolbar_right_button),
                       isDescendantOfA(withId(org.chromium.chrome.tab_ui.R.id.bottom_controls))))
                .perform(click());
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.search_box_text))
                .check(matches(isCompletelyDisplayed()))
                .perform(replaceText("wfh tips"));
        onView(withId(org.chromium.chrome.start_surface.R.id.url_bar))
                .perform(pressKey(KeyEvent.KEYCODE_ENTER));

        // Verify a tab is created within the group by checking the tab strip and tab model.
        onView(withId(org.chromium.chrome.tab_ui.R.id.toolbar_container_view))
                .check(waitForView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                        isCompletelyDisplayed())));
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view),
                       withParent(withId(org.chromium.chrome.tab_ui.R.id.toolbar_container_view))))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(4));
        assertEquals(4, cta.getTabModelSelector().getCurrentModel().getCount());
        assertEquals(1, filter.getTabGroupCount());
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS +
        "/home_button_on_grid_tab_switcher/true"})
    public void testHomeButtonOnTabSwitcher() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        StartSurfaceTestUtils.clickMoreTabs(cta);
        waitForView(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view));
        onView(withId(org.chromium.chrome.tab_ui.R.id.home_button_on_tab_switcher))
                .check(matches(isDisplayed()));
        HomeButton homeButton =
                cta.findViewById(org.chromium.chrome.tab_ui.R.id.home_button_on_tab_switcher);
        Assert.assertFalse(homeButton.isLongClickable());
        onView(withId(org.chromium.chrome.start_surface.R.id.home_button_on_tab_switcher))
                .perform(click());

        onView(withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/show_tabs_in_mru_order/true"})
    public void test_CarouselTabSwitcherShowTabsInMRUOrder() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager() != null && cta.getLayoutManager().overviewVisible());
        StartSurfaceTestUtils.waitForTabModel(cta);
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.logo));
        Tab tab1 = cta.getCurrentTabModel().getTabAt(0);

        // Launches the first site in MV tiles.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        Tab tab2 = cta.getActivityTab();
        // Verifies that the titles of the two Tabs are different.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertNotEquals(tab1.getTitle(), tab2.getTitle()); });

        // Returns to the Start surface.
        OverviewModeBehaviorWatcher overviewModeWatcher =
                new OverviewModeBehaviorWatcher(cta.getLayoutManager(), true, false);
        StartSurfaceTestUtils.pressHomePageButton(cta);
        overviewModeWatcher.waitForBehavior();
        waitForView(allOf(
                withParent(withId(org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container)),
                withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)));

        RecyclerView recyclerView = cta.findViewById(org.chromium.chrome.tab_ui.R.id.tab_list_view);
        assertEquals(2, recyclerView.getChildCount());
        // Verifies that the tabs are shown in MRU order: the first card in the carousel Tab
        // switcher is the last created Tab by tapping the MV tile; the second card is the Tab
        // created or restored in setup().
        RecyclerView.ViewHolder firstViewHolder = recyclerView.findViewHolderForAdapterPosition(0);
        TextView title1 =
                firstViewHolder.itemView.findViewById(org.chromium.chrome.tab_ui.R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab2.getTitle(), title1.getText()));

        RecyclerView.ViewHolder secondViewHolder = recyclerView.findViewHolderForAdapterPosition(1);
        TextView title2 =
                secondViewHolder.itemView.findViewById(org.chromium.chrome.tab_ui.R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab1.getTitle(), title2.getText()));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/show_tabs_in_mru_order/true"})
    public void testShow_GridTabSwitcher_AlwaysShowTabsInCreationOrder() {
        tabSwitcher_AlwaysShowTabsInGridTabSwitcherInCreationOrderImpl();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS +
        "/show_tabs_in_mru_order/true/show_last_active_tab_only/true"})
    public void testShowV2_GridTabSwitcher_AlwaysShowTabsInCreationOrder() {
        // clang-format on
        tabSwitcher_AlwaysShowTabsInGridTabSwitcherInCreationOrderImpl();
    }

    private void tabSwitcher_AlwaysShowTabsInGridTabSwitcherInCreationOrderImpl() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> cta.getLayoutManager() != null && cta.getLayoutManager().overviewVisible());
        StartSurfaceTestUtils.waitForTabModel(cta);
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.logo));
        Tab tab1 = cta.getCurrentTabModel().getTabAt(0);

        // Launches the first site in MV tiles.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        Tab tab2 = cta.getActivityTab();

        // Verifies that the titles of the two Tabs are different.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertNotEquals(tab1.getTitle(), tab2.getTitle()); });

        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        // Enter the Tab switcher.
        TabUiTestHelper.enterTabSwitcher(cta);
        waitForView(
                allOf(withParent(withId(
                              org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view)),
                        withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)));

        ViewGroup secondaryTaskSurface = cta.findViewById(
                org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view);
        RecyclerView recyclerView =
                secondaryTaskSurface.findViewById(org.chromium.chrome.tab_ui.R.id.tab_list_view);
        assertEquals(2, recyclerView.getChildCount());
        // Verifies that the tabs are shown in MRU order: the first card in the Tab switcher is the
        // last created Tab by tapping the MV tile; the second card is the Tab created or restored
        // in setup().
        RecyclerView.ViewHolder firstViewHolder = recyclerView.findViewHolderForAdapterPosition(0);
        TextView title1 =
                firstViewHolder.itemView.findViewById(org.chromium.chrome.tab_ui.R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab1.getTitle(), title1.getText()));

        RecyclerView.ViewHolder secondViewHolder = recyclerView.findViewHolderForAdapterPosition(1);
        TextView title2 =
                secondViewHolder.itemView.findViewById(org.chromium.chrome.tab_ui.R.id.tab_title);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(tab2.getTitle(), title2.getText()));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testDoNotInitializeSecondaryTasksSurfaceWithoutOpenGridTabSwitcher() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(startSurfaceCoordinator.isSecondaryTasksSurfaceEmptyForTesting());
        });
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(startSurfaceCoordinator.isSecondaryTasksSurfaceEmptyForTesting());
        });

        TabUiTestHelper.enterTabSwitcher(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(startSurfaceCoordinator.isSecondaryTasksSurfaceEmptyForTesting());
        });
    }

    /**
     * @return Whether both features {@link ChromeFeatureList#INSTANT_START} and
     * {@link ChromeFeatureList#TAB_SWITCHER_ON_RETURN} are enabled.
     */
    private boolean isInstantReturn() {
        return mUseInstantStart && mImmediateReturn;
    }
}