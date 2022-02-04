// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_BASE_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.sClassParamsForStartSurfaceTest;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

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
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.SuggestionsTileView;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tasks.MvTilesLayout;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests of MV tiles on the {@link StartSurface}.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class StartSurfaceMVTilesTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = sClassParamsForStartSurfaceTest;
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

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
    private FakeMostVisitedSites mMostVisitedSites;

    public StartSurfaceMVTilesTest(boolean useInstantStart, boolean immediateReturn) {
        CachedFeatureFlags.setForTesting(ChromeFeatureList.INSTANT_START, useInstantStart);

        mUseInstantStart = useInstantStart;
        mImmediateReturn = immediateReturn;
    }

    @Before
    public void setUp() throws IOException {
        mMostVisitedSites = StartSurfaceTestUtils.setMVTiles(mSuggestionsDeps);

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
    public void testTapMVTilesInSingleSurface() {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        if (isInstantReturn()) {
            // TODO(crbug.com/1076274): fix toolbar to avoid wrongly focusing on the toolbar
            // omnibox.
            return;
        }
        // Press back button should close the tab opened from the Start surface.
        OverviewModeBehaviorWatcher showWatcher = TabUiTestHelper.createOverviewShowWatcher(cta);
        StartSurfaceTestUtils.pressBack(mActivityTestRule);
        showWatcher.waitForBehavior();
        assertThat(cta.getTabModelSelector().getCurrentModel().getCount(), equalTo(1));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testCleanUpMVTilesAfterHiding() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(startSurfaceCoordinator.isMVTilesCleanedUpForTesting());
        });

        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        Assert.assertEquals("The launched tab should have the launch type FROM_START_SURFACE",
                TabLaunchType.FROM_START_SURFACE,
                cta.getActivityTabProvider().get().getLaunchType());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(startSurfaceCoordinator.isMVTilesCleanedUpForTesting());
        });
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testMVTilesInitialized() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        if (!mImmediateReturn) StartSurfaceTestUtils.pressHomePageButton(cta);
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(cta);
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);

        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(startSurfaceCoordinator.isMVTilesInitializedForTesting());
        });

        TabUiTestHelper.enterTabSwitcher(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(startSurfaceCoordinator.isMVTilesInitializedForTesting());
        });
    }

    /* MV tiles context menu tests starts. */
    @Test
    @MediumTest
    @Feature({"StartSurface"})
    // Disable feed placeholder animation because it causes waitForSnackbar() to time out.
    @CommandLineFlags.
    Add({START_SURFACE_TEST_BASE_PARAMS, FeedPlaceholderLayout.DISABLE_ANIMATION_SWITCH})
    public void testDismissTileWithContextMenuAndUndo() throws Exception {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        SiteSuggestion siteToDismiss = mMostVisitedSites.getCurrentSites().get(1);
        final View tileView = getTileViewFor(siteToDismiss);

        // Dismiss the tile using the context menu.
        invokeContextMenu(tileView, ContextMenuManager.ContextMenuItemId.REMOVE);
        Assert.assertTrue(mMostVisitedSites.isUrlBlocklisted(siteToDismiss.url));

        // Ensure that the removal is reflected in the ui.
        Assert.assertEquals(8, getMvTilesLayout().getChildCount());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mMostVisitedSites.setTileSuggestions(getNewSitesAfterDismiss(siteToDismiss)));
        waitForTileRemoved(siteToDismiss);
        Assert.assertEquals(7, getMvTilesLayout().getChildCount());

        // Undo the dismiss through snack bar.
        final View snackbarButton = waitForSnackbar();
        Assert.assertTrue(mMostVisitedSites.isUrlBlocklisted(siteToDismiss.url));
        TestThreadUtils.runOnUiThreadBlocking((Runnable) snackbarButton::callOnClick);
        Assert.assertFalse(mMostVisitedSites.isUrlBlocklisted(siteToDismiss.url));
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testOpenTileInNewTabWithContextMenu() throws ExecutionException {
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);

        SiteSuggestion siteToOpen = mMostVisitedSites.getCurrentSites().get(1);
        final View tileView = getTileViewFor(siteToOpen);

        // Open the tile using the context menu.
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        invokeContextMenu(tileView, ContextMenuManager.ContextMenuItemId.OPEN_IN_NEW_TAB);
        // This tab should be opened in the background.
        Assert.assertTrue(cta.getLayoutManager().overviewVisible());
        // Verifies a new tab is created.
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_TEST_BASE_PARAMS})
    public void testOpenTileInIncognitoTabWithContextMenu() throws ExecutionException {
        Assume.assumeFalse("https://crbug.com/1210554", mUseInstantStart && mImmediateReturn);
        if (!mImmediateReturn) {
            StartSurfaceTestUtils.pressHomePageButton(mActivityTestRule.getActivity());
        }
        StartSurfaceTestUtils.waitForOverviewVisible(
                mLayoutChangedCallbackHelper, mCurrentlyActiveLayout);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);

        SiteSuggestion siteToOpen = mMostVisitedSites.getCurrentSites().get(1);
        final View tileView = getTileViewFor(siteToOpen);

        // Open the incognito tile using the context menu.
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        invokeContextMenu(tileView, ContextMenuManager.ContextMenuItemId.OPEN_IN_INCOGNITO_TAB);
        hideWatcher.waitForBehavior();
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
        // Verifies a new incognito tab is created.
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 1);
    }
    /* MV tiles context menu tests ends. */

    /**
     * @return Whether both features {@link ChromeFeatureList#INSTANT_START} and
     * {@link ChromeFeatureList#TAB_SWITCHER_ON_RETURN} are enabled.
     */
    private boolean isInstantReturn() {
        return mUseInstantStart && mImmediateReturn;
    }

    private View getTileViewFor(SiteSuggestion suggestion) {
        onViewWaiting(
                allOf(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout), isDisplayed()));
        View tileView = getMvTilesLayout().getTileViewForTesting(suggestion);
        Assert.assertNotNull("Tile not found for suggestion " + suggestion.url, tileView);

        return tileView;
    }

    private MvTilesLayout getMvTilesLayout() {
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout));
        MvTilesLayout mvTilesLayout = mActivityTestRule.getActivity().findViewById(
                org.chromium.chrome.tab_ui.R.id.mv_tiles_layout);
        Assert.assertNotNull("Unable to retrieve the MvTilesLayout.", mvTilesLayout);
        return mvTilesLayout;
    }

    private void waitForTileRemoved(final SiteSuggestion suggestion) throws TimeoutException {
        MvTilesLayout mvTilesLayout = getMvTilesLayout();
        final SuggestionsTileView removedTile = mvTilesLayout.getTileViewForTesting(suggestion);
        if (removedTile == null) return;

        final CallbackHelper callback = new CallbackHelper();
        mvTilesLayout.setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() {
            @Override
            public void onChildViewAdded(View parent, View child) {}

            @Override
            public void onChildViewRemoved(View parent, View child) {
                if (child == removedTile) callback.notifyCalled();
            }
        });
        callback.waitForCallback("The expected tile was not removed.", 0);
        mvTilesLayout.setOnHierarchyChangeListener(null);
    }

    private void invokeContextMenu(View view, int contextMenuItemId) throws ExecutionException {
        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), view);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), contextMenuItemId, 0));
    }

    private List<SiteSuggestion> getNewSitesAfterDismiss(SiteSuggestion siteToDismiss) {
        List<SiteSuggestion> newSites = new ArrayList<>();
        for (SiteSuggestion site : mMostVisitedSites.getCurrentSites()) {
            if (!site.url.equals(siteToDismiss.url)) {
                newSites.add(site);
            }
        }
        return newSites;
    }

    /** Wait for the snackbar associated to a tile dismissal to be shown and returns its button. */
    private View waitForSnackbar() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        final String expectedSnackbarMessage = cta.getResources().getString(
                org.chromium.chrome.start_surface.R.string.most_visited_item_removed);
        CriteriaHelper.pollUiThread(() -> {
            SnackbarManager snackbarManager = cta.getSnackbarManager();
            Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(true));
            TextView snackbarMessage =
                    cta.findViewById(org.chromium.chrome.start_surface.R.id.snackbar_message);
            Criteria.checkThat(snackbarMessage, Matchers.notNullValue());
            Criteria.checkThat(
                    snackbarMessage.getText().toString(), Matchers.is(expectedSnackbarMessage));
        });

        return cta.findViewById(org.chromium.chrome.start_surface.R.id.snackbar_button);
    }
}
