// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressKey;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_BASE_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.sClassParamsForStartSurfaceTest;
import static org.chromium.ui.test.util.ViewUtils.VIEW_GONE;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.view.KeyEvent;
import android.view.View;

import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Assume;
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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorTestingRobot;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Integration tests of the back action when {@link StartSurface} is enabled.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class StartSurfaceBackButtonTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = sClassParamsForStartSurfaceTest;

    private static final long MAX_TIMEOUT_MS = 40000L;

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
    public StartSurfaceBackButtonTest(boolean useInstantStart, boolean immediateReturn) {
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
    public void testShow_SingleAsHomepage_BackButton() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);

        // Case 1:
        // Launches the first site in mv tiles, and press back button.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        StartSurfaceTestUtils.pressBack(mActivityTestRule);

        CriteriaHelper.pollUiThread(() -> cta.getLayoutManager().overviewVisible());
        // Verifies the new Tab is deleted.
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Case 2:
        // Launches the first site in mv tiles, and press home button to return to the Start
        // surface.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        StartSurfaceTestUtils.pressHomePageButton(cta);
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view));
        onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view), isDisplayed()));

        // Launches the new tab from the carousel tab switcher, and press back button.
        StartSurfaceTestUtils.clickTabInCarousel(/* position = */ 1);
        Assert.assertEquals(TabLaunchType.FROM_START_SURFACE,
                cta.getTabModelSelector().getCurrentTab().getLaunchType());
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view));
        // Verifies the tab isn't auto deleted from the TabModel.
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    @DisableIf.
        Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "Flaky, see crbug.com/1246457")
    public void testShow_SingleAsHomepage_BackButtonWithTabSwitcher() {
        // clang-format on
        singleAsHomepage_BackButtonWithTabSwitcher();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/show_last_active_tab_only/true"})
    @DisableIf.
        Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "Flaky, see crbug.com/1246457")
    public void testShow_SingleAsHomepageV2_BackButtonWithTabSwitcher() {
        // clang-format on
        singleAsHomepage_BackButtonWithTabSwitcher();
    }

    private void singleAsHomepage_BackButtonWithTabSwitcher() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_container), isDisplayed()));

        // Launches the first site in mv tiles.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);

        if (isInstantReturn()
                && (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
                        && Build.VERSION.SDK_INT < Build.VERSION_CODES.O)) {
            // Fix the issue that failed to perform a single click on the tab switcher button.
            // See code below.
            return;
        }

        // Enters the tab switcher, and choose the new tab. After the tab is opening, press back.
        waitForView(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_button));
        TabUiTestHelper.enterTabSwitcher(cta);
        waitForView(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view));
        waitForView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view));
        onView(allOf(withParent(withId(org.chromium.chrome.tab_ui.R.id.tasks_surface_body)),
                       withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(1, click()));
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        Assert.assertEquals(TabLaunchType.FROM_START_SURFACE,
                cta.getTabModelSelector().getCurrentTab().getLaunchType());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertTrue(StartSurfaceUserData.getKeepTab(
                                cta.getTabModelSelector().getCurrentTab())));

        OverviewModeBehaviorWatcher overviewModeWatcher =
                new OverviewModeBehaviorWatcher(cta.getLayoutManager(), true, false);
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        // Verifies the new Tab isn't deleted, and Start surface is shown.
        overviewModeWatcher.waitForBehavior();
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        // Verifies Chrome is closed.
        try {
            StartSurfaceTestUtils.pressBack(mActivityTestRule);
        } catch (Exception e) {
        } finally {
            CriteriaHelper.pollUiThread(
                    ()
                            -> {
                        return ActivityLifecycleMonitorRegistry.getInstance().getLifecycleStageOf(
                                       cta)
                                == Stage.STOPPED;
                    },
                    "Tapping back button should close Chrome.", MAX_TIMEOUT_MS,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        }
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    @DisableIf.
        Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "Flaky, see crbug.com/1246457")
    public void testShow_SingleAsHomepage_BackButtonOnCarouselTabSwitcher() {
        // clang-format on
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.search_box_text))
                .perform(replaceText("about:blank"));
        onView(withId(org.chromium.chrome.start_surface.R.id.url_bar))
                .perform(pressKey(KeyEvent.KEYCODE_ENTER));
        hideWatcher.waitForBehavior();
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        TabUiTestHelper.mergeAllNormalTabsToAGroup(cta);
        StartSurfaceTestUtils.pressHomePageButton(cta);
        CriteriaHelper.pollUiThread(() -> cta.getLayoutManager().overviewVisible());

        StartSurfaceTestUtils.clickFirstTabInCarousel();
        onViewWaiting(allOf(
                withId(org.chromium.chrome.tab_ui.R.id.dialog_container_view), isDisplayed()));

        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        waitForView(withId(org.chromium.chrome.tab_ui.R.id.dialog_container_view), VIEW_GONE);
        onView(withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view))
                .check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testShow_SingleAsHomepage_BackButtonOnTabSwitcherWithDialogShowing() {
        backButtonOnTabSwitcherWithDialogShowingImpl();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/show_last_active_tab_only/true"})
    public void testShow_SingleAsHomepageV2_BackButtonOnTabSwitcherWithDialogShowing() {
        backButtonOnTabSwitcherWithDialogShowingImpl();
    }

    private void backButtonOnTabSwitcherWithDialogShowingImpl() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.logo));

        // Launches the first site in mv tiles.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);

        List<Tab> tabs = getTabsInCurrentTabModel(cta.getCurrentTabModel());
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();

        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_button));
        TabUiTestHelper.enterTabSwitcher(cta);

        waitForView(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view));
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> startSurfaceCoordinator.showTabSelectionEditorForTesting(tabs));
        robot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(
                        org.chromium.chrome.tab_ui.R.string.tab_selection_editor_group)
                .verifyToolbarSelectionTextWithResourceId(
                        org.chromium.chrome.tab_ui.R.string
                                .tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(tabs.size())
                .verifyHasAtLeastNItemVisible(2);

        // Verifies that tapping the back button will close the TabSelectionEditor.
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view));

        // Groups the two tabs.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> startSurfaceCoordinator.showTabSelectionEditorForTesting(tabs));
        robot.resultRobot.verifyToolbarActionButtonWithResourceId(
                org.chromium.chrome.tab_ui.R.string.tab_selection_editor_group);
        robot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarActionButton();
        robot.resultRobot.verifyTabSelectionEditorIsHidden();

        // Opens the TabGridDialog by clicking the first group card.
        onViewWaiting(Matchers.allOf(withParent(withId(
                                             org.chromium.chrome.tab_ui.R.id.tasks_surface_body)),
                              withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollUiThread(() -> isTabGridDialogShown(cta));

        // Verifies that the TabGridDialog is closed by tapping back button.
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        CriteriaHelper.pollUiThread(() -> isTabGridDialogHidden(cta));
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view));
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testShow_SingleAsHomepage_BackButtonOnHomepageWithGroupTabsDialog() {
        backButtonOnHomepageWithGroupTabsDialogImpl();
    }

    @Test
    @LargeTest
    @Feature({"StartSurface"})
    @EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS + "/show_last_active_tab_only/true"})
    public void testShow_SingleAsHomepageV2_BackButtonOnHomepageWithGroupTabsDialog() {
        backButtonOnHomepageWithGroupTabsDialogImpl();
    }

    private void backButtonOnHomepageWithGroupTabsDialogImpl() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.logo));

        // Launches the first site in MV tiles to create the second tab for grouping.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);

        // When show_last_active_tab_only is enabled, we need to enter the tab switcher first to
        // initialize the secondary task surface which shows the TabSelectionEditor dialog.
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.tab_switcher_button));
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        TabUiTestHelper.enterTabSwitcher(cta);
        waitForView(withId(org.chromium.chrome.start_surface.R.id.secondary_tasks_surface_view));
        List<Tab> tabs = getTabsInCurrentTabModel(cta.getCurrentTabModel());
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();

        // Enters the homepage, and shows the TabSelectionEditor dialog.
        StartSurfaceTestUtils.pressHomePageButton(cta);
        waitForView(withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view));

        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> startSurfaceCoordinator.showTabSelectionEditorForTesting(tabs));
        robot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(
                        org.chromium.chrome.tab_ui.R.string.tab_selection_editor_group)
                .verifyToolbarSelectionTextWithResourceId(
                        org.chromium.chrome.tab_ui.R.string
                                .tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(tabs.size())
                .verifyHasAtLeastNItemVisible(2);

        // Verifies that tapping the back button will close the TabSelectionEditor.
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
        onViewWaiting(withId(org.chromium.chrome.start_surface.R.id.primary_tasks_surface_view));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testOpenRecentTabOnStartAndTapBackButtonReturnToStartSurface()
            throws ExecutionException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        // Taps on the "Recent tabs" menu item.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), cta,
                org.chromium.chrome.R.id.recent_tabs_menu_id);
        CriteriaHelper.pollUiThread(() -> cta.getActivityTabProvider().get() != null);
        Assert.assertEquals("The launched tab should have the launch type FROM_START_SURFACE",
                TabLaunchType.FROM_START_SURFACE,
                cta.getActivityTabProvider().get().getLaunchType());
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        StartSurfaceTestUtils.pressBack(mActivityTestRule);

        // Tap the back on the "Recent tabs" should take us back to the start surface homepage, and
        // the Tab should be deleted.
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // clang-format off
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    @DisableIf.
        Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "Flaky, see crbug.com/1246457")
    public void testSwipeBackOnStartSurfaceHomePage() throws ExecutionException {
        // clang-format on
        // TODO(https://crbug.com/1093632): Requires 2 back press/gesture events now. Make this
        // work with a single event.
        Assume.assumeFalse(mImmediateReturn);
        StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);

        StartSurfaceTestUtils.gestureNavigateBack(mActivityTestRule);

        // Back gesture on the start surface puts Chrome background.
        ChromeApplicationTestUtils.waitUntilChromeInBackground();
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    @DisabledTest(message = "https://crbug.com/1281915")
    public void testSwipeBackOnTabOfLaunchTypeStartSurface() throws ExecutionException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        Assert.assertEquals("The launched tab should have the launch type FROM_START_SURFACE",
                TabLaunchType.FROM_START_SURFACE,
                cta.getActivityTabProvider().get().getLaunchType());

        StartSurfaceTestUtils.gestureNavigateBack(mActivityTestRule);

        // Back gesture on the tab should take us back to the start surface homepage.
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
    }

    /**
     * @return Whether both features {@link ChromeFeatureList#INSTANT_START} and
     * {@link ChromeFeatureList#TAB_SWITCHER_ON_RETURN} are enabled.
     */
    private boolean isInstantReturn() {
        return mUseInstantStart && mImmediateReturn;
    }

    private List<Tab> getTabsInCurrentTabModel(TabModel currentTabModel) {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            tabs.add(currentTabModel.getTabAt(i));
        }
        return tabs;
    }

    private boolean isTabGridDialogShown(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(org.chromium.chrome.tab_ui.R.id.dialog_parent_view);
        return dialogView.getVisibility() == View.VISIBLE && dialogView.getAlpha() == 1f;
    }

    private boolean isTabGridDialogHidden(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(org.chromium.chrome.tab_ui.R.id.dialog_parent_view);
        return dialogView.getVisibility() == View.GONE;
    }
}
