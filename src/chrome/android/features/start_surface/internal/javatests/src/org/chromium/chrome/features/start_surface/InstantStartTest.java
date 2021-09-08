// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Build;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.core.AllOf;
import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ErrorCollector;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.NativeLibraryLoadedStatus;
import org.chromium.base.SysUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesMetadataUtils;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Integration tests of Instant Start which requires 2-stage initialization for Clank startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
        ChromeFeatureList.START_SURFACE_ANDROID, ChromeFeatureList.INSTANT_START})
@Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
public class InstantStartTest {
    // clang-format on
    private static final String IMMEDIATE_RETURN_PARAMS = "force-fieldtrial-params=Study.Group:"
            + ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS_PARAM + "/0";
    private static final int ARTICLE_SECTION_HEADER_POSITION = 0;
    private static final long MAX_TIMEOUT_MS = 30000L;
    private static final String SHADOW_VIEW_TAG = "TabListViewShadow";
    private Bitmap mBitmap;
    private int mThumbnailFetchCount;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

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
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS})
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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
            ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
            ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single/enable_launch_polish/true"})
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
        startAndWaitNativeInitialization();
        Assert.assertFalse(startSurfaceCoordinator.isInitPendingForTesting());
        Assert.assertFalse(startSurfaceCoordinator.isSecondaryTaskInitPendingForTesting());
        Assert.assertTrue(startSurfaceCoordinator.isInitializedWithNativeForTesting());
    }

    /**
     * Tests that the IncognitoSwitchCoordinator isn't create in inflate() if the native library
     * isn't ready. It will be lazily created after native initialization.
     */
    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void startSurfaceToolbarInflatedPreAndWithNativeTest() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(cta.isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));

        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        TopToolbarCoordinator topToolbarCoordinator =
                (TopToolbarCoordinator) cta.getToolbarManager().getToolbar();

        onViewWaiting(allOf(withId(R.id.tab_switcher_toolbar), isDisplayed()));

        StartSurfaceToolbarCoordinator startSurfaceToolbarCoordinator =
                topToolbarCoordinator.getStartSurfaceToolbarForTesting();
        // Verifies that the TabCountProvider for incognito toggle tab layout hasn't been set when
        // the {@link StartSurfaceToolbarCoordinator#inflate()} is called.
        Assert.assertNull(
                startSurfaceToolbarCoordinator.getIncognitoToggleTabCountProviderForTesting());

        // Initializes native.
        startAndWaitNativeInitialization();
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> startSurfaceToolbarCoordinator
                                   .getIncognitoToggleTabCountProviderForTesting()
                        != null);
    }

    /**
     * Tests that clicking the "more_tabs" button won't make Omnibox get focused when single tab is
     * shown on the StartSurface.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS +
            "/start_surface_variation/single/show_last_active_tab_only/true"})
    public void startSurfaceMoreTabsButtonTest() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(cta.isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(-1));
        Assert.assertTrue(StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY.getValue());

        mActivityTestRule.waitForActivityNativeInitializationComplete();

        StartSurfaceTestUtils.clickMoreTabs(cta);

        onViewWaiting(
                allOf(withParent(withId(R.id.tasks_surface_body)), withId(R.id.tab_list_view)));
        Assert.assertFalse(cta.findViewById(R.id.url_bar).isFocused());
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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:start_surface_variation/single"})
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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    @DisabledTest(message = "Test doesn't work with FeedV2. FeedV1 is removed crbug.com/1165828.")
    public void renderTabSwitcher() throws IOException, InterruptedException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1, 2});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(2);
        TabAttributeCache.setTitleForTesting(0, "title");
        TabAttributeCache.setTitleForTesting(1, "漢字");
        TabAttributeCache.setTitleForTesting(2, "اَلْعَرَبِيَّةُ");

        // Must be after StartSurfaceTestUtils.createTabStateFile() to read these files.
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
        CriteriaHelper.pollUiThread(() -> allCardsHaveThumbnail(recyclerView));
        mRenderTestRule.render(recyclerView, "tabSwitcher_3tabs");

        // Resume native initialization and make sure the GTS looks the same.
        startAndWaitNativeInitialization();

        Assert.assertEquals(3, cta.getTabModelSelector().getCurrentModel().getCount());
        // TODO(crbug.com/1065314): find a better way to wait for a stable rendering.
        Thread.sleep(2000);
        // The titles on the tab cards changes to "Google" because we use M26_GOOGLE_COM.
        mRenderTestRule.render(recyclerView, "tabSwitcher_3tabs_postNative");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    @DisableIf.Build(message = "Flaky. See https://crbug.com/1091311",
            sdk_is_greater_than = Build.VERSION_CODES.O)
    public void renderTabGroups() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(2);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(3);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(4);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        TabAttributeCache.setRootIdForTesting(2, 0);
        TabAttributeCache.setRootIdForTesting(3, 3);
        TabAttributeCache.setRootIdForTesting(4, 3);
        // StartSurfaceTestUtils.createTabStateFile() has to be after setRootIdForTesting() to get
        // root IDs.
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1, 2, 3, 4});

        // Must be after StartSurfaceTestUtils.createTabStateFile() to read these files.
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
        CriteriaHelper.pollUiThread(() -> allCardsHaveThumbnail(recyclerView));
        // TODO(crbug.com/1065314): Tab group cards should not have favicons.
        mRenderTestRule.render(
                cta.findViewById(R.id.tab_list_view), "tabSwitcher_tabGroups_aspect_ratio_point85");

        // Resume native initialization and make sure the GTS looks the same.
        startAndWaitNativeInitialization();

        Assert.assertEquals(5, cta.getTabModelSelector().getCurrentModel().getCount());
        Assert.assertEquals(2,
                cta.getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()
                        .getCount());
        Assert.assertEquals(3,
                getRelatedTabListSizeOnUiThread(cta.getTabModelSelector()
                                                        .getTabModelFilterProvider()
                                                        .getCurrentTabModelFilter()));
        // TODO(crbug.com/1065314): fix thumbnail changing in post-native rendering and make sure
        //  post-native GTS looks the same.
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        "force-fieldtrials=Study/Group",
        IMMEDIATE_RETURN_PARAMS +
            "/start_surface_variation/single" +
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
        startAndWaitNativeInitialization();

        // TODO(crbug.com/1065314): fix login button animation in post-native rendering and
        //  make sure post-native single-tab card looks the same.
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testFeedPlaceholderFromColdStart() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        Assert.assertFalse(mActivityTestRule.getActivity().isTablet());
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));

        // Feed placeholder should be shown from cold start with Instant Start on.
        onView(withId(R.id.placeholders_layout)).check(matches(isDisplayed()));
        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());

        startAndWaitNativeInitialization();
        // Feed background should be non-transparent finally.
        ViewUtils.onViewWaiting(
                AllOf.allOf(withId(R.id.feed_stream_recycler_view), matchesBackgroundAlpha(255)));

        // TODO(spdonghao): Add a test for Feed placeholder from warm start. It's tested in
        // StartSurfaceMediatorUnitTest#feedPlaceholderFromWarmStart currently because warm start is
        // hard to simulate here.
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testCachedFeedVisibility() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        // FEED_ARTICLES_LIST_VISIBLE should equal to ARTICLES_LIST_VISIBLE.
        CriteriaHelper.pollUiThread(()
                                            -> UserPrefs.get(Profile.getLastUsedRegularProfile())
                                                       .getBoolean(Pref.ARTICLES_LIST_VISIBLE)
                        == StartSurfaceConfiguration.getFeedArticlesVisibility());

        // Hide articles and verify that FEED_ARTICLES_LIST_VISIBLE and ARTICLES_LIST_VISIBLE are
        // both false.
        toggleHeader(false);
        CriteriaHelper.pollUiThread(() -> !StartSurfaceConfiguration.getFeedArticlesVisibility());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals(UserPrefs.get(Profile.getLastUsedRegularProfile())
                                                       .getBoolean(Pref.ARTICLES_LIST_VISIBLE),
                                StartSurfaceConfiguration.getFeedArticlesVisibility()));

        // Show articles and verify that FEED_ARTICLES_LIST_VISIBLE and ARTICLES_LIST_VISIBLE are
        // both true.
        toggleHeader(true);
        CriteriaHelper.pollUiThread(StartSurfaceConfiguration::getFeedArticlesVisibility);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals(UserPrefs.get(Profile.getLastUsedRegularProfile())
                                                       .getBoolean(Pref.ARTICLES_LIST_VISIBLE),
                                StartSurfaceConfiguration.getFeedArticlesVisibility()));
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testHideFeedPlaceholder() {
        // clang-format on
        StartSurfaceConfiguration.setFeedVisibilityForTesting(false);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);

        // When cached Feed articles' visibility is invisible, placeholder should be invisible too.
        onView(withId(R.id.placeholders_layout)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testShowFeedPlaceholder() {
        // clang-format on
        StartSurfaceConfiguration.setFeedVisibilityForTesting(true);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);

        // When cached Feed articles' visibility is visible, placeholder should be visible too.
        onView(withId(R.id.placeholders_layout)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @Features.DisableFeatures(ChromeFeatureList.START_SURFACE_ANDROID)
    public void testInstantStartWithoutStartSurface() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertTrue(TabUiFeatureUtilities.supportInstantStart(false));
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertFalse(ReturnToChromeExperimentsUtil.isStartSurfaceHomepageEnabled());

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertEquals(1, cta.getTabModelSelector().getCurrentModel().getCount());
        Layout activeLayout = cta.getLayoutManager().getActiveLayout();
        Assert.assertTrue(activeLayout instanceof StaticLayout);
        Assert.assertEquals(123, ((StaticLayout) activeLayout).getCurrentTabIdForTesting());
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.
    Add({ChromeSwitches.ENABLE_ACCESSIBILITY_TAB_SWITCHER, "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    // clang-format off
    public void testInstantStartNotCrashWhenAccessibilityLayoutEnabled() throws IOException {
        // clang-format on
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));
        Assert.assertTrue(DeviceClassManager.enableAccessibilityLayout());

        StartSurfaceTestUtils.createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertFalse(TabUiFeatureUtilities.supportInstantStart(false));
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertTrue(ReturnToChromeExperimentsUtil.isStartSurfaceHomepageEnabled());

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertEquals(1, cta.getTabModelSelector().getCurrentModel().getCount());
        Layout activeLayout = cta.getLayoutManager().getActiveLayout();
        Assert.assertTrue(activeLayout instanceof StaticLayout);
        Assert.assertEquals(123, ((StaticLayout) activeLayout).getCurrentTabIdForTesting());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    // clang-format off
    public void  testInstantStartDisabledOnLowEndDevice() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {123});
        mActivityTestRule.startMainActivityFromLauncher();
        // SysUtils.resetForTesting is required here due to the test restriction setup. With the
        // RESTRICTION_TYPE_NON_LOW_END_DEVICE restriction on the class, SysUtils#detectLowEndDevice
        // is called before the BaseSwitches.ENABLE_LOW_END_DEVICE_MODE is applied. Reset here to
        // make sure BaseSwitches.ENABLE_LOW_END_DEVICE_MODE can be applied.
        SysUtils.resetForTesting();

        Assert.assertFalse(TabUiFeatureUtilities.supportInstantStart(false));
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:start_surface_variation/single"})
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
        ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePageNoTabs(
                mActivityTestRule.getActivity());
        PseudoTab.getAllPseudoTabsFromStateFile(mActivityTestRule.getActivity());

        Assert.assertFalse("There should be no GURL usages triggering native library loading",
                NativeLibraryLoadedStatus.getProviderForTesting().areMainDexNativeMethodsReady());
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void renderSingleAsHomepage_NoTab_scrollToolbarToTop() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        // Initializes native.
        startAndWaitNativeInitialization();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);

        StartSurfaceTestUtils.scrollToolbar(cta);

        View surface = cta.findViewById(R.id.control_container);
        ChromeRenderTestRule.sanitize(surface);
        mRenderTestRule.render(surface, "singlePane_floatingTopToolbar");

        // Focus the omnibox.
        UrlBar urlBar = cta.findViewById(org.chromium.chrome.R.id.url_bar);
        TestThreadUtils.runOnUiThreadBlocking((Runnable) urlBar::requestFocus);
        // Clear the focus.
        TestThreadUtils.runOnUiThreadBlocking(urlBar::clearFocus);
        // Default search engine logo should still show.
        surface = cta.findViewById(R.id.control_container);
        ChromeRenderTestRule.sanitize(surface);
        mRenderTestRule.render(surface, "singlePane_floatingTopToolbar");
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testShadowVisibility() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        onView(withId(R.id.toolbar_shadow))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.INVISIBLE)));

        startAndWaitNativeInitialization();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        onView(withId(R.id.toolbar_shadow))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.INVISIBLE)));
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testShadowVisibilityWithoutInstantStart() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        onViewWaiting(withId(R.id.toolbar_shadow)).check(matches(isDisplayed()));

        startAndWaitNativeInitialization();
        StartSurfaceTestUtils.waitForOverviewVisible(mActivityTestRule.getActivity());

        onView(withId(R.id.toolbar_shadow))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.INVISIBLE)));
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testSingleAsHomepage_CloseTabInCarouselTabSwitcher()
            throws IOException, ExecutionException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        // Initializes native.
        startAndWaitNativeInitialization();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);
        onView(allOf(withParent(withId(R.id.carousel_tab_switcher_container)),
                       withId(R.id.tab_list_view)))
                .check(matches(isDisplayed()));
        RecyclerView tabListView = cta.findViewById(R.id.tab_list_view);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tabListView.getChildAt(0).findViewById(R.id.action_button).performClick());

        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);
        assertEquals(cta.findViewById(R.id.tab_switcher_title).getVisibility(), View.GONE);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void renderSingleAsHomepage_Landscape() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.setMVTiles(mSuggestionsDeps);

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        // Initializes native.
        startAndWaitNativeInitialization();
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
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testSingleAsHomepage_Landscape_TabSize() {
        // clang-format on
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        // Initializes native.
        startAndWaitNativeInitialization();
        onViewWaiting(
                allOf(withId(org.chromium.chrome.start_surface.R.id.feed_stream_recycler_view),
                        isDisplayed()));

        // Rotate to landscape mode.
        ActivityTestUtils.rotateActivityToOrientation(cta, ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    cta.getResources().getConfiguration().orientation, is(ORIENTATION_LANDSCAPE));
        });

        // Launch the first MV tile to open a tab.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 0);
        StartSurfaceTestUtils.pressHomePageButton(cta);

        // Wait for thumbnail to show.
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        onViewWaiting(allOf(withId(R.id.tab_thumbnail), isDisplayed()));

        View tabThumbnail = cta.findViewById(R.id.tab_thumbnail);
        float defaultRatio = (float) TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO.getValue();
        defaultRatio = MathUtils.clamp(defaultRatio, 0.5f, 2.0f);
        assertEquals(tabThumbnail.getMeasuredHeight(),
                (int) (tabThumbnail.getMeasuredWidth() * 1.0 / defaultRatio), 2);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.EXPLORE_SITES})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS +
                    "/start_surface_variation/single/exclude_mv_tiles/false"})
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
        startAndWaitNativeInitialization();

        StartSurfaceTestUtils.waitForTabModel(cta);

        // When showing MV tiles pre-native, explore top sites view is already rendered with a
        // non-null icon. This test is for ensuring explore top sites view (locating at the first
        // tile) is built and clickable after native initialization.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 0);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void testScrollToSelectedTab() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, null, 5);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);
        startAndWaitNativeInitialization();
        StartSurfaceTestUtils.waitForOverviewVisible(cta);

        StartSurfaceTestUtils.clickMoreTabs(cta);
        onViewWaiting(withId(R.id.secondary_tasks_surface_view));

        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.tasks_surface_body))))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;
                    Assert.assertTrue(v instanceof RecyclerView);
                    LinearLayoutManager layoutManager =
                            (LinearLayoutManager) ((RecyclerView) v).getLayoutManager();
                    assertEquals(7, layoutManager.findLastVisibleItemPosition());
                });

        // On tab switcher page, shadow is handled by TabListRecyclerView itself, so toolbar shadow
        // shouldn't show.
        onView(withId(R.id.toolbar_shadow)).check(matches(not(isDisplayed())));

        // Scroll the tab list a little bit and shadow should show.
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.tasks_surface_body))))
                .perform(swipeUp());
        onView(allOf(withTagValue(is(SHADOW_VIEW_TAG)),
                       isDescendantOfA(withId(R.id.tasks_surface_body))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
            ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION,
            "force-fieldtrials=Study/Group",
            IMMEDIATE_RETURN_PARAMS + "/start_surface_variation/single"})
    public void doNotRestoreEmptyTabs() throws IOException {
        // clang-format on
        StartSurfaceTestUtils.createTabStateFile(
                new int[] {0, 1}, new String[] {"", "about:blank"});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(1);
        TabAttributeCache.setTitleForTesting(0, "");
        TabAttributeCache.setTitleForTesting(0, "Google");

        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.waitForOverviewVisible(mActivityTestRule.getActivity());
        ViewUtils.onViewWaiting(withId(R.id.tab_list_view));
        Assert.assertEquals(
                1, PseudoTab.getAllPseudoTabsFromStateFile(mActivityTestRule.getActivity()).size());
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS +
        "/start_surface_variation/single/omnibox_focused_on_new_tab/true"})
    public void testNewTabFromLauncher() throws IOException {
        // clang-format on
        testNewTabFromLauncherImpl();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS +
        "/start_surface_variation/single/omnibox_focused_on_new_tab/true"})
    public void testNewTabFromLauncherWithInstantStartDisabled() throws IOException {
        // clang-format on
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewTabFromLauncherImpl();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS +
        "/start_surface_variation/single"})
    public void testNewTabFromLauncherWithHomepageDisabled_NoFinale() throws IOException {
        // clang-format on
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewTabFromLauncherWithHomepageDisabledImpl();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS +
        "/start_surface_variation/single"})
    public void testNewTabFromLauncherWithHomepageDisabled_NoFinale_NoInstant() throws IOException {
        // clang-format on
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewTabFromLauncherWithHomepageDisabledImpl();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS +
        "/start_surface_variation/single/omnibox_focused_on_new_tab/true"})
    public void testNewTabFromLauncherWithHomepageDisabled_Finale() throws IOException {
        // clang-format on
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewTabFromLauncherWithHomepageDisabledImpl();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS +
        "/start_surface_variation/single/omnibox_focused_on_new_tab/true"})
    public void testNewTabFromLauncherWithHomepageDisabled_Finale_NoInstant() throws IOException {
        // clang-format on
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewTabFromLauncherWithHomepageDisabledImpl();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS +
        "/start_surface_variation/single"})
    public void testShowTabSwitcherWhenHomepageDisabled() throws IOException {
        // clang-format on
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals(0, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
        testShowTabSwitcherWhenHomepageDisabledWithImmediateReturnImpl();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", IMMEDIATE_RETURN_PARAMS +
        "/start_surface_variation/single"})
    public void testShowTabSwitcherWhenHomepageDisabled_NoInstant() throws IOException {
        // clang-format on
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals(0, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
        testShowTabSwitcherWhenHomepageDisabledWithImmediateReturnImpl();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", "force-fieldtrial-params=Study.Group:" +
        "start_surface_variation/single"})
    public void testShowLastTabWhenHomepageDisabledNoImmediateReturn() throws IOException {
        // clang-format on
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals(-1, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
        testShowLastTabWhenHomepageDisabledNoImmediateReturnImpl();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
        ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", "force-fieldtrial-params=Study.Group:" +
        "start_surface_variation/single"})
    public void testShowLastTabWhenHomepageDisabledNoImmediateReturn_NoInstant()
          throws IOException {
        // clang-format on
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        Assert.assertEquals(-1, ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS.getValue());
        testShowLastTabWhenHomepageDisabledNoImmediateReturnImpl();
    }

    private void testNewTabFromLauncherImpl() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        startNewTabFromLauncherIcon();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        waitForView(withId(org.chromium.chrome.start_surface.R.id.search_box_text));
        TextView urlBar = cta.findViewById(org.chromium.chrome.start_surface.R.id.url_bar);
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(org.chromium.chrome.start_surface.R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(cta.findViewById(org.chromium.chrome.start_surface.R.id.toolbar_buttons)
                             .getVisibility(),
                View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                cta.getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });
    }

    private void testNewTabFromLauncherWithHomepageDisabledImpl() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        HomepageManager.getInstance().setPrefHomepageEnabled(false);
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

        // Launches new Tab from the launcher icon and verifies that a NTP is created and showing.
        startNewTabFromLauncherIcon();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        Assert.assertFalse(cta.getLayoutManager().overviewVisible());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(UrlUtilities.isNTPUrl(cta.getActivityTab().getUrl())); });
    }

    private void testShowTabSwitcherWhenHomepageDisabledWithImmediateReturnImpl()
            throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        HomepageManager.getInstance().setPrefHomepageEnabled(false);
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

        // Launches Chrome and verifies that the Tab switcher is showing.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 0);

        Assert.assertTrue(cta.getLayoutManager().overviewVisible());
        StartSurfaceCoordinator startSurfaceCoordinator =
                StartSurfaceTestUtils.getStartSurfaceFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(startSurfaceCoordinator.getController().getStartSurfaceState(),
                    StartSurfaceState.NOT_SHOWN);
        });
    }

    private void testShowLastTabWhenHomepageDisabledNoImmediateReturnImpl() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        HomepageManager.getInstance().setPrefHomepageEnabled(false);
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

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

    private void startNewTabFromLauncherIcon() {
        Intent intent = IntentHandler.createTrustedOpenNewTabIntent(
                ContextUtils.getApplicationContext(), false);
        intent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, true);
        mActivityTestRule.prepareUrlIntent(intent, null);
        mActivityTestRule.launchActivity(intent);
    }

    private void startAndWaitNativeInitialization() {
        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());

        CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().startDelayedNativeInitializationForTests());
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized,
                10000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
    }

    private boolean allCardsHaveThumbnail(RecyclerView recyclerView) {
        RecyclerView.Adapter adapter = recyclerView.getAdapter();
        assert adapter != null;
        for (int i = 0; i < adapter.getItemCount(); i++) {
            RecyclerView.ViewHolder viewHolder = recyclerView.findViewHolderForAdapterPosition(i);
            if (viewHolder != null) {
                ImageView thumbnail = viewHolder.itemView.findViewById(R.id.tab_thumbnail);
                if (!(thumbnail.getDrawable() instanceof BitmapDrawable)) return false;
                BitmapDrawable drawable = (BitmapDrawable) thumbnail.getDrawable();
                Bitmap bitmap = drawable.getBitmap();
                if (bitmap == null) return false;
            }
        }
        return true;
    }

    private int getRelatedTabListSizeOnUiThread(TabModelFilter tabModelFilter) {
        AtomicInteger res = new AtomicInteger();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { res.set(tabModelFilter.getRelatedTabList(2).size()); });
        return res.get();
    }

    /**
     * Toggles the header and checks whether the header has the right status.
     *
     * @param expanded Whether the header should be expanded.
     */
    private void toggleHeader(boolean expanded) {
        onView(allOf(instanceOf(RecyclerView.class), withId(R.id.feed_stream_recycler_view)))
                .perform(RecyclerViewActions.scrollToPosition(ARTICLE_SECTION_HEADER_POSITION));
        onView(withId(R.id.header_menu)).perform(click());

        onView(withText(expanded ? R.string.ntp_turn_on_feed : R.string.ntp_turn_off_feed))
                .perform(click());

        onView(withText(expanded ? R.string.ntp_discover_on : R.string.ntp_discover_off))
                .check(matches(isDisplayed()));
    }

    public static Matcher<View> matchesBackgroundAlpha(final int expectedAlpha) {
        return new BoundedMatcher<View, View>(View.class) {
            String mMessage;
            int mActualAlpha;

            @Override
            protected boolean matchesSafely(View item) {
                if (item.getBackground() == null) {
                    mMessage = item.getId() + " does not have a background";
                    return false;
                }
                mActualAlpha = item.getBackground().getAlpha();
                return mActualAlpha == expectedAlpha;
            }
            @Override
            public void describeTo(final Description description) {
                if (expectedAlpha != mActualAlpha) {
                    mMessage = "Background alpha did not match: Expected " + expectedAlpha + " was "
                            + mActualAlpha;
                }
                description.appendText(mMessage);
            }
        };
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
