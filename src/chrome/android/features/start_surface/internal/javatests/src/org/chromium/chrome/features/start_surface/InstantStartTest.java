// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.notNullValue;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.INSTANT_START_TEST_BASE_PARAMS;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.text.TextUtils;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.core.AllOf;
import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ErrorCollector;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.NativeLibraryLoadedStatus;
import org.chromium.base.SysUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.feed.FeedPlaceholderLayout;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesMetadataUtils;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.test.util.ViewUtils;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;

/**
 * Integration tests of Instant Start which requires 2-stage initialization for Clank startup. See
 * {@link InstantStartToolbarTest}, {@link InstantStartFeedTest}, {@link
 * InstantStartTabSwitcherTest}, {@link InstantStartNewTabFromLauncherTest} for more tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.
    Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
        ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.INSTANT_START})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE,
    UiRestriction.RESTRICTION_TYPE_PHONE})
public class InstantStartTest {
    // clang-format on
    private static final String IMMEDIATE_RETURN_PARAMS = "force-fieldtrial-params=Study.Group:"
            + ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0";
    private static final String START_PARAMS =
            "force-fieldtrial-params=Study.Group:start_surface_variation/single";
    private Bitmap mBitmap;
    private int mThumbnailFetchCount;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().setRevision(1).build();

    @Rule
    public ErrorCollector collector = new ErrorCollector();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @After
    public void tearDown() {
        if (mActivityTestRule.getActivity() != null) {
            ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        }
    }

    /**
     * Test TabContentManager is able to fetch thumbnail jpeg files before native is initialized.
     */
    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    // clang-format off
    @Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE,
        UiRestriction.RESTRICTION_TYPE_PHONE, UiRestriction.RESTRICTION_TYPE_TABLET})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrial-params=Study.Group:allow_to_refetch/true/thumbnail_aspect_ratio/2.0"})
    public void fetchThumbnailsPreNativeTest() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        Assert.assertTrue(TabContentManager.ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION.getValue());

        int tabId = 0;
        mThumbnailFetchCount = 0;
        Callback<Bitmap> thumbnailFetchListener = (bitmap) -> {
            mThumbnailFetchCount++;
            mBitmap = bitmap;
        };

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    mActivityTestRule.getActivity().getTabContentManager(), notNullValue());
        });

        TabContentManager tabContentManager =
                mActivityTestRule.getActivity().getTabContentManager();

        final Bitmap thumbnailBitmap =
                StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(tabId);
        tabContentManager.getTabThumbnailWithCallback(tabId, thumbnailFetchListener, false, false);
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(mThumbnailFetchCount, greaterThan(0)));

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        Assert.assertEquals(1, mThumbnailFetchCount);
        Bitmap fetchedThumbnail = mBitmap;
        Assert.assertEquals(thumbnailBitmap.getByteCount(), fetchedThumbnail.getByteCount());
        Assert.assertEquals(thumbnailBitmap.getWidth(), fetchedThumbnail.getWidth());
        Assert.assertEquals(thumbnailBitmap.getHeight(), fetchedThumbnail.getHeight());
    }

    @Test
    @SmallTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION, IMMEDIATE_RETURN_PARAMS})
    public void layoutManagerChromePhonePreNativeTest() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(cta.isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));

        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        assertThat(cta.getLayoutManager()).isInstanceOf(LayoutManagerChromePhone.class);
        assertThat(cta.getLayoutManager().getOverviewLayout())
                .isInstanceOf(StartSurfaceLayout.class);
    }

    @Test
    @SmallTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
            ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
            ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS + "/enable_launch_polish/true"})
    public void startSurfaceSinglePanePreNativeAndWithNativeTest() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(cta.isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));

        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        assertThat(cta.getLayoutManager()).isInstanceOf(LayoutManagerChromePhone.class);
        assertThat(cta.getLayoutManager().getOverviewLayout())
                .isInstanceOf(StartSurfaceLayout.class);

        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        Assert.assertTrue(startSurfaceCoordinator.isInitPendingForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isInitializedWithNativeForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            startSurfaceCoordinator.getController().setOverviewState(
                    StartSurfaceState.SHOWN_TABSWITCHER);
        });
        CriteriaHelper.pollUiThread(startSurfaceCoordinator::isSecondaryTaskInitPendingForTesting);

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        Assert.assertFalse(startSurfaceCoordinator.isInitPendingForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting());
        Assert.assertTrue(startSurfaceCoordinator.isInitializedWithNativeForTesting());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void willInitNativeOnTabletTest() {
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertTrue(cta.isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));

        CriteriaHelper.pollUiThread(
                () -> { Criteria.checkThat(cta.getLayoutManager(), notNullValue()); });

        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
        assertThat(cta.getLayoutManager()).isInstanceOf(LayoutManagerChromeTablet.class);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION, START_PARAMS})
    public void testShouldShowStartSurfaceAsTheHomePagePreNative() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        Assert.assertTrue(StartSurfaceConfiguration.isStartSurfaceSinglePaneEnabled());
        Assert.assertFalse(TextUtils.isEmpty(HomepageManager.getHomepageUri()));

        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) ()
                        -> ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePage(
                                mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        // Disable feed placeholder animation because we can't render it in exactly the same way
        // for each run.
        FeedPlaceholderLayout.DISABLE_ANIMATION_SWITCH,
            INSTANT_START_TEST_BASE_PARAMS +
            "/exclude_mv_tiles/true" +
            "/hide_switch_when_no_incognito_tabs/true" +
            "/show_last_active_tab_only/true"})
    public void renderSingleAsHomepage_SingleTabNoMVTiles()
        throws IOException {
        // clang-format on

        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        View surface = cta.findViewById(R.id.primary_tasks_surface_view);

        ViewUtils.onViewWaiting(AllOf.allOf(withId(R.id.single_tab_view), isDisplayed()));
        ChromeRenderTestRule.sanitize(surface);
        // TODO(crbug.com/1065314): fix favicon.
        mRenderTestRule.render(surface, "singlePane_singleTab_noMV4_FeedV2");

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);

        // TODO(crbug.com/1065314): fix login button animation in post-native rendering and
        //  make sure post-native single-tab card looks the same.
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.START_SURFACE_ANDROID)
    public void testInstantStartWithoutStartSurface() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertTrue(
                TabUiFeatureUtilities.supportInstantStart(false, mActivityTestRule.getActivity()));
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(ReturnToChromeExperimentsUtil.isStartSurfaceEnabled(cta));

        Assert.assertEquals(1, cta.getTabModelSelector().getCurrentModel().getCount());
        Layout activeLayout = cta.getLayoutManager().getActiveLayout();
        Assert.assertTrue(activeLayout instanceof StaticLayout);
        Assert.assertEquals(123, ((StaticLayout) activeLayout).getCurrentTabIdForTesting());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.ENABLE_ACCESSIBILITY_TAB_SWITCHER,
        INSTANT_START_TEST_BASE_PARAMS})
    public void testInstantStartNotCrashWhenAccessibilityLayoutEnabled() throws IOException {
        // clang-format on
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));

        StartSurfaceTestUtils.createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();
        Assert.assertTrue(
                DeviceClassManager.enableAccessibilityLayout(mActivityTestRule.getActivity()));

        Assert.assertFalse(
                TabUiFeatureUtilities.supportInstantStart(false, mActivityTestRule.getActivity()));
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(ReturnToChromeExperimentsUtil.isStartSurfaceEnabled(cta));
        Assert.assertEquals(1, cta.getTabModelSelector().getCurrentModel().getCount());
        Layout activeLayout = cta.getLayoutManager().getActiveLayout();
        Assert.assertTrue(activeLayout instanceof StaticLayout);
        Assert.assertEquals(123, ((StaticLayout) activeLayout).getCurrentTabIdForTesting());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @DisabledTest(message = "Failing at least on M, O and P: https://crbug.com/1254327")
    public void testInstantStartDisabledOnLowEndDevice() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();
        // SysUtils.resetForTesting is required here due to the test restriction setup. With the
        // RESTRICTION_TYPE_NON_LOW_END_DEVICE restriction on the class, SysUtils#detectLowEndDevice
        // is called before the BaseSwitches.ENABLE_LOW_END_DEVICE_MODE is applied. Reset here to
        // make sure BaseSwitches.ENABLE_LOW_END_DEVICE_MODE can be applied.
        SysUtils.resetForTesting();

        Assert.assertFalse(
                TabUiFeatureUtilities.supportInstantStart(false, mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION, START_PARAMS})
    public void testNoGURLPreNative() {
        // clang-format on
        if (!BuildConfig.ENABLE_ASSERTS) return;
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        collector.checkThat(StartSurfaceConfiguration.isStartSurfaceSinglePaneEnabled(), is(true));
        collector.checkThat(TextUtils.isEmpty(HomepageManager.getHomepageUri()), is(false));
        Assert.assertFalse(
                NativeLibraryLoadedStatus.getProviderForTesting().areMainDexNativeMethodsReady());
        ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePage(
                mActivityTestRule.getActivity());
        ReturnToChromeExperimentsUtil.isStartSurfaceEnabled(mActivityTestRule.getActivity());
        PseudoTab.getAllPseudoTabsFromStateFile(mActivityTestRule.getActivity());

        Assert.assertFalse("There should be no GURL usages triggering native library loading",
                NativeLibraryLoadedStatus.getProviderForTesting().areMainDexNativeMethodsReady());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        INSTANT_START_TEST_BASE_PARAMS})
    public void renderSingleAsHomepage_Landscape() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.setMVTiles(mSuggestionsDeps);

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        onViewWaiting(
                allOf(withId(org.chromium.chrome.start_surface.R.id.feed_stream_recycler_view),
                        isDisplayed()));

        cta.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    cta.getResources().getConfiguration().orientation, is(ORIENTATION_LANDSCAPE));
        });
        View surface = cta.findViewById(R.id.primary_tasks_surface_view);
        mRenderTestRule.render(surface, "singlePane_landscapeV2");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.EXPLORE_SITES})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            // Disable feed placeholder animation because we can't render it in exactly the same
            // way for each run.
            FeedPlaceholderLayout.DISABLE_ANIMATION_SWITCH,
            INSTANT_START_TEST_BASE_PARAMS + "/exclude_mv_tiles/false"})
    public void testMVTilesWithExploreSitesView() throws InterruptedException, IOException {
        // clang-format on
        FakeMostVisitedSites mostVisitedSites = StartSurfaceTestUtils.setMVTiles(mSuggestionsDeps);
        saveSiteSuggestionTilesToFile(mostVisitedSites.getCurrentSites());
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        View surface = cta.findViewById(R.id.primary_tasks_surface_view);

        ViewUtils.onViewWaiting(
                allOf(withId(R.id.tile_view_title), withText("0 EXPLORE_SITES"), isDisplayed()));
        ChromeRenderTestRule.sanitize(surface);

        // Render MV tiles pre-native to make sure MV tiles background icons are inflated.
        mRenderTestRule.render(surface, "singlePane_MV_withExploreSitesViewV2");

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);

        StartSurfaceTestUtils.waitForTabModel(cta);

        // When showing MV tiles pre-native, explore top sites view is already rendered with a
        // non-null icon. This test is for ensuring explore top sites view (locating at the first
        // tile) is built and clickable after native initialization.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 0);
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({START_PARAMS})
    public void testShowLastTabWhenHomepageDisabledNoImmediateReturn() throws IOException {
        // clang-format on
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals(-1, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(false));
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

        testShowLastTabAtStartUp();
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({START_PARAMS})
    public void testShowLastTabWhenHomepageDisabledNoImmediateReturn_NoInstant()
          throws IOException {
        // clang-format on
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals(-1, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(false));
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

        testShowLastTabAtStartUp();
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS +
        "/hide_start_when_last_visited_tab_is_srp/true"})
    public void testShowLastTabWhenLastTabIsSRP_NoInstant() throws IOException {
        // clang-format on
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP, true);
        testShowLastTabAtStartUp();
    }

    private void testShowLastTabAtStartUp() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        // Launches Chrome and verifies that the last visited Tab is showing.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        Assert.assertFalse(cta.getLayoutManager().overviewVisible());
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(startSurfaceCoordinator.getController().getStartSurfaceState(),
                    StartSurfaceState.NOT_SHOWN);
        });
    }

    private static List<Tile> createFakeSiteSuggestionTiles(List<SiteSuggestion> siteSuggestions) {
        List<Tile> suggestionTiles = new ArrayList<>();
        for (int i = 0; i < siteSuggestions.size(); i++) {
            suggestionTiles.add(new Tile(siteSuggestions.get(i), i));
        }
        return suggestionTiles;
    }

    private static void saveSiteSuggestionTilesToFile(List<SiteSuggestion> siteSuggestions)
            throws InterruptedException {
        // Get old file and ensure to delete it.
        File oldFile = MostVisitedSitesMetadataUtils.getOrCreateTopSitesDirectory();
        Assert.assertTrue(oldFile.delete() && !oldFile.exists());

        // Save suggestion lists to file.
        final CountDownLatch latch = new CountDownLatch(1);
        MostVisitedSitesMetadataUtils.saveSuggestionListsToFile(
                createFakeSiteSuggestionTiles(siteSuggestions), latch::countDown);

        // Wait util the file has been saved.
        latch.await();
    }
}
