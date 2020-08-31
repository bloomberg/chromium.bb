// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static android.os.Build.VERSION_CODES.N_MR1;
import static android.os.Build.VERSION_CODES.O_MR1;
import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static android.support.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.areAnimatorsEnabled;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.leaveTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.rotateDeviceToOrientation;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.switchTabModel;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabModelTabCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;
import static org.chromium.components.embedder_support.util.UrlConstants.NTP_URL;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.NoMatchingViewException;
import android.support.test.espresso.ViewAssertion;
import android.support.test.espresso.contrib.AccessibilityChecks;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.StaticLayout;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorTestingRobot;
import org.chromium.chrome.browser.tasks.tab_management.TabSuggestionMessageService;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.widget.ChipView;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

// clang-format off
/** Tests for the {@link StartSurfaceLayout} */
@SuppressWarnings("ConstantConditions")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study",
        ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
public class StartSurfaceLayoutTest {
    // clang-format on
    private static final String BASE_PARAMS = "force-fieldtrial-params="
            + "Study.Group:soft-cleanup-delay/0/cleanup-delay/0/skip-slow-zooming/false"
            + "/zooming-min-sdk-version/19/zooming-min-memory-mb/512";

    // Tests need animation on.
    @ClassRule
    public static DisableAnimationsTestRule sEnableAnimationsRule =
            new DisableAnimationsTestRule(true);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    @SuppressWarnings("FieldCanBeLocal")
    private EmbeddedTestServer mTestServer;
    private StartSurfaceLayout mStartSurfaceLayout;
    private String mUrl;
    private int mRepeat;
    private List<WeakReference<Bitmap>> mAllBitmaps = new LinkedList<>();
    private Callback<Bitmap> mBitmapListener =
            (bitmap) -> mAllBitmaps.add(new WeakReference<>(bitmap));
    private TabSwitcher.TabListDelegate mTabListDelegate;

    @Before
    public void setUp() {
        AccessibilityChecks.enable();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        // After setUp, Chrome is launched and has one NTP.
        mActivityTestRule.startMainActivityFromLauncher();

        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        mStartSurfaceLayout = (StartSurfaceLayout) layout;
        mUrl = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        mRepeat = 1;

        mTabListDelegate = mStartSurfaceLayout.getStartSurfaceForTesting().getTabListDelegate();
        mTabListDelegate.setBitmapCallbackForTesting(mBitmapListener);
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting());

        mActivityTestRule.getActivity().getTabContentManager().setCaptureMinRequestTimeForTesting(
                0);

        CriteriaHelper.pollUiThread(Criteria.equals(true,
                mActivityTestRule.getActivity()
                        .getTabModelSelector()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter()::isTabModelRestored));

        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting());
    }

    @After
    public void tearDown() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRenderGrid_3WebTabs() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(3, 0, "about:blank");
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        // See crbug.com/1063619
        mRenderTestRule.setPixelDiffThreshold(2);
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "3_web_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(sdk_is_greater_than = O_MR1, message = "crbug.com/1077552")
    public void testRenderGrid_10WebTabs() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(10, 0, "about:blank");
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        // See crbug.com/1063619
        mRenderTestRule.setPixelDiffThreshold(2);
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "10_web_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(sdk_is_greater_than = O_MR1, message = "crbug.com/1077552")
    public void testRenderGrid_10WebTabs_InitialScroll() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(10, 0, "about:blank");
        assertEquals(9, cta.getTabModelSelector().getCurrentModel().index());
        enterGTSWithThumbnailRetry();
        // See crbug.com/1063619
        mRenderTestRule.setPixelDiffThreshold(2);
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "10_web_tabs-select_last");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRenderGrid_Incognito() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Prepare some incognito tabs and enter tab switcher.
        prepareTabs(1, 3, "about:blank");
        assertTrue(cta.getCurrentTabModel().isIncognito());
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        // See crbug.com/1063619
        mRenderTestRule.setPixelDiffThreshold(2);
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "3_incognito_web_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study"})
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRenderGrid_3NativeTabs() throws IOException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Prepare some incognito native tabs and enter tab switcher.
        // NTP in incognito mode is chosen for its consistency in look, and we don't have to mock
        // away the MV tiles, login promo, feed, etc.
        prepareTabs(1, 3, null);
        assertTrue(cta.getCurrentTabModel().isIncognito());
        // Make sure all thumbnails are there before switching tabs.
        enterGTSWithThumbnailRetry();
        leaveTabSwitcher(cta);
        // Espresso.pressBack();

        ChromeTabUtils.switchTabInCurrentTabModel(cta, 0);
        enterTabSwitcher(cta);
        mRenderTestRule.render(cta.findViewById(R.id.tab_list_view), "3_incognito_ntps");
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testTabToGridFromLiveTab() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        assertEquals(0, mTabListDelegate.getSoftCleanupDelayForTesting());
        assertEquals(0, mTabListDelegate.getCleanupDelayForTesting());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @DisabledTest(message = "crbug.com/991852 This test is flaky")
    public void testTabToGridFromLiveTabAnimation() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/2000/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabWarm() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        assertEquals(2000, mTabListDelegate.getSoftCleanupDelayForTesting());
        assertEquals(10000, mTabListDelegate.getCleanupDelayForTesting());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/2000/cleanup-delay/10000"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.M) // TODO(crbug.com/997065#c8): remove SDK restriction.
    public void testTabToGridFromLiveTabWarmAnimation() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    public void testTabToGridFromLiveTabSoft() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        assertEquals(0, mTabListDelegate.getSoftCleanupDelayForTesting());
        assertEquals(10000, mTabListDelegate.getCleanupDelayForTesting());

        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.M) // TODO(crbug.com/997065#c8): remove SDK restriction.
    public void testTabToGridFromLiveTabSoftAnimation() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(mUrl);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    @FlakyTest(message = "crbug.com/1075804")
    public void testTabToGridFromNtp() throws InterruptedException {
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(NTP_URL);
    }

    /**
     * Make Chrome have {@code numTabs} of regular Tabs and {@code numIncognitoTabs} of incognito
     * tabs with {@code url} loaded, and assert no bitmap fetching occurred.
     *
     * @param numTabs The number of regular tabs.
     * @param numIncognitoTabs The number of incognito tabs.
     * @param url The URL to load.
     */
    private void prepareTabs(int numTabs, int numIncognitoTabs, @Nullable String url) {
        int oldCount = mTabListDelegate.getBitmapFetchCountForTesting();
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, numTabs, numIncognitoTabs, url);
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - oldCount);
    }

    private void testTabToGrid(String fromUrl) throws InterruptedException {
        mActivityTestRule.loadUrl(fromUrl);

        final int initCount = getCaptureCount();

        for (int i = 0; i < mRepeat; i++) {
            enterGTSWithThumbnailChecking();
            leaveGTSAndVerifyThumbnailsAreReleased();
        }
        checkFinalCaptureCount(false, initCount);
    }

    @Test
    @MediumTest
    public void testGridToTabToCurrentNTP() throws InterruptedException {
        prepareTabs(1, 0, NTP_URL);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    public void testGridToTabToOtherNTP() throws InterruptedException {
        prepareTabs(2, 0, NTP_URL);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    public void testGridToTabToCurrentLive() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(1, 0, mUrl);
        testGridToTab(false, false);
    }

    // From https://stackoverflow.com/a/21505193
    private static boolean isEmulator() {
        return Build.FINGERPRINT.startsWith("generic") || Build.FINGERPRINT.startsWith("unknown")
                || Build.MODEL.contains("google_sdk") || Build.MODEL.contains("Emulator")
                || Build.MODEL.contains("Android SDK built for x86")
                || Build.MANUFACTURER.contains("Genymotion")
                || (Build.BRAND.startsWith("generic") && Build.DEVICE.startsWith("generic"))
                || "google_sdk".equals(Build.PRODUCT);
    }

    /**
     * Test that even if there are tabs with stuck pending thumbnail readback, it doesn't block
     * thumbnail readback for the current tab.
     */
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    public void testGridToTabToCurrentLiveDetached() throws Exception {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        // This works on emulators but not on real devices. See crbug.com/986047.
        if (!isEmulator()) return;

        for (int i = 0; i < 10; i++) {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            // Quickly create some tabs, navigate to web pages, and don't wait for thumbnail
            // capturing.
            mActivityTestRule.loadUrl(mUrl);
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), cta, false, false);
            mActivityTestRule.loadUrl(mUrl);
            // Hopefully we are in a state where some pending readbacks are stuck because their tab
            // is not attached to the view.
            if (cta.getTabContentManager().getPendingReadbacksForTesting() > 0) {
                break;
            }

            // Restart Chrome.
            // Although we're destroying the activity, the Application will still live on since its
            // in the same process as this test.
            TestThreadUtils.runOnUiThreadBlocking(() -> cta.getTabModelSelector().closeAllTabs());
            TabUiTestHelper.finishActivity(cta);
            mActivityTestRule.startMainActivityOnBlankPage();
            assertEquals(1, mActivityTestRule.tabsCount(false));
        }
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertNotEquals(0, cta.getTabContentManager().getPendingReadbacksForTesting());
        assertEquals(1, cta.getCurrentTabModel().index());

        // The last tab should still get thumbnail even though readbacks for other tabs are stuck.
        enterTabSwitcher(cta);
        TabUiTestHelper.checkThumbnailsExist(cta.getTabModelSelector().getCurrentTab());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @DisabledTest(message = "crbug.com/993201 This test fails deterministically on Nexus 5X")
    public void testGridToTabToCurrentLiveWithAnimation() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(1, 0, mUrl);
        testGridToTab(false, false);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    public void testGridToTabToOtherLive() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @DisabledTest(message = "crbug.com/993201 This test fails deterministically on Nexus 5X")
    public void testGridToTabToOtherLiveWithAnimation() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, false);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    public void testGridToTabToOtherFrozen() throws InterruptedException {
        assertFalse(TabUiFeatureUtilities.isTabToGtsAnimationEnabled());
        prepareTabs(2, 0, mUrl);
        testGridToTab(true, true);
    }

    private void testGridToTab(boolean switchToAnotherTab, boolean killBeforeSwitching)
            throws InterruptedException {
        final int initCount = getCaptureCount();

        for (int i = 0; i < mRepeat; i++) {
            enterGTSWithThumbnailChecking();

            final int index = mActivityTestRule.getActivity().getCurrentTabModel().index();
            final int targetIndex = switchToAnotherTab ? 1 - index : index;
            Tab targetTab =
                    mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(targetIndex);
            if (killBeforeSwitching) {
                WebContentsUtils.simulateRendererKilled(targetTab.getWebContents(), false);
            }

            if (switchToAnotherTab) {
                waitForCaptureRateControl();
            }
            int count = getCaptureCount();
            onView(withId(R.id.tab_list_view))
                    .perform(actionOnItemAtPosition(targetIndex, click()));
            CriteriaHelper.pollUiThread(() -> {
                boolean doneHiding =
                        !mActivityTestRule.getActivity().getLayoutManager().overviewVisible();
                if (!doneHiding) {
                    // Before overview hiding animation is done, the tab index should not change.
                    assertEquals(
                            index, mActivityTestRule.getActivity().getCurrentTabModel().index());
                }
                return doneHiding;
            }, "Overview not hidden yet");
            int delta;
            if (switchToAnotherTab
                    && !TextUtils.equals(mActivityTestRule.getActivity()
                                                 .getCurrentWebContents()
                                                 .getLastCommittedUrl(),
                            NTP_URL)) {
                // Capture the original tab.
                delta = 1;
            } else {
                delta = 0;
            }
            checkCaptureCount(delta, count);
        }
        checkFinalCaptureCount(switchToAnotherTab, initCount);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRestoredTabsDontFetch() throws Exception {
        prepareTabs(2, 0, mUrl);
        int oldCount = mTabListDelegate.getBitmapFetchCountForTesting();

        // Restart Chrome.
        // Although we're destroying the activity, the Application will still live on since its in
        // the same process as this test.
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityOnBlankPage();
        assertEquals(3, mActivityTestRule.tabsCount(false));

        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        mStartSurfaceLayout = (StartSurfaceLayout) layout;
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - oldCount);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testInvisibleTabsDontFetch() throws InterruptedException {
        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/soft-cleanup-delay/2000/cleanup-delay/10000"})
    @FlakyTest(message = "crbug.com/1075804")
    public void testInvisibleTabsDontFetchWarm() throws InterruptedException {
        // Get the GTS in the warm state.
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(NTP_URL);

        Thread.sleep(1000);

        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000"})
    @FlakyTest(message = "crbug.com/1075804")
    public void testInvisibleTabsDontFetchSoft() throws InterruptedException {
        // Get the GTS in the soft cleaned up state.
        prepareTabs(2, 0, NTP_URL);
        testTabToGrid(NTP_URL);

        Thread.sleep(1000);

        // Open a few new tabs.
        final int count = mTabListDelegate.getBitmapFetchCountForTesting();
        for (int i = 0; i < 3; i++) {
            MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(), org.chromium.chrome.R.id.new_tab_menu_id);
        }
        // Fetching might not happen instantly.
        Thread.sleep(1000);

        // No fetching should happen.
        assertEquals(0, mTabListDelegate.getBitmapFetchCountForTesting() - count);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisabledTest(message = "http://crbug/1005865 - Test was previously flaky but only on bots."
            + "Was not locally reproducible. Disabling until verified that it's deflaked on bots.")
    public void testIncognitoEnterGts() throws InterruptedException {
        prepareTabs(1, 1, null);
        enterGTSWithThumbnailChecking();
        onView(withId(R.id.tab_list_view))
                .check(TabCountAssertion.havingTabCount(1));

        onView(withId(R.id.tab_list_view)).perform(actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollUiThread(
                () -> !mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        enterGTSWithThumbnailChecking();
        onView(withId(R.id.tab_list_view))
                .check(TabCountAssertion.havingTabCount(1));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testIncognitoToggle_tabCount() {
        mActivityTestRule.loadUrl(mUrl);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        // Prepare two incognito tabs and enter tab switcher.
        createTabs(cta, true, 2);
        enterTabSwitcher(cta);
        onView(withId(R.id.tab_list_view))
                .check(TabCountAssertion.havingTabCount(2));

        for (int i = 0; i < mRepeat; i++) {
            switchTabModel(cta, false);
            onView(withId(R.id.tab_list_view))
                    .check(TabCountAssertion.havingTabCount(1));

            switchTabModel(cta, true);
            onView(withId(R.id.tab_list_view))
                    .check(TabCountAssertion.havingTabCount(2));
        }
        leaveGTSAndVerifyThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.M,
            message = "https://crbug.com/1023833")
    public void testIncognitoToggle_thumbnailFetchCount() throws InterruptedException {
        mActivityTestRule.loadUrl(mUrl);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        int oldFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();

        // Prepare two incognito tabs and enter tab switcher.
        prepareTabs(1, 2, mUrl);
        enterGTSWithThumbnailChecking();

        int currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
        assertEquals(2, currentFetchCount - oldFetchCount);
        oldFetchCount = currentFetchCount;
        int oldHistogramRecord = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_JPEG);

        for (int i = 0; i < mRepeat; i++) {
            switchTabModel(cta, false);
            currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
            int currentHistogramRecord = RecordHistogram.getHistogramValueCountForTesting(
                    TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                    TabContentManager.ThumbnailFetchingResult.GOT_JPEG);
            assertEquals(1, currentFetchCount - oldFetchCount);
            assertEquals(1, currentHistogramRecord - oldHistogramRecord);
            oldFetchCount = currentFetchCount;
            oldHistogramRecord = currentHistogramRecord;

            switchTabModel(cta, true);
            currentFetchCount = mTabListDelegate.getBitmapFetchCountForTesting();
            currentHistogramRecord = RecordHistogram.getHistogramValueCountForTesting(
                    TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                    TabContentManager.ThumbnailFetchingResult.GOT_JPEG);
            assertEquals(2, currentFetchCount - oldFetchCount);
            assertEquals(2, currentHistogramRecord - oldHistogramRecord);
            oldFetchCount = currentFetchCount;
            oldHistogramRecord = currentHistogramRecord;
        }
        leaveGTSAndVerifyThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
                    ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUrlUpdatedNotCrashing_ForUndoableClosedTab() throws Exception {
        // clang-format on
        mActivityTestRule.getActivity().getSnackbarManager().disableForTesting();
        prepareTabs(2, 0, null);
        enterGTSWithThumbnailChecking();

        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().closeTab(
                    tab, false, false, true);
        });
        mActivityTestRule.loadUrlInTab(
                mUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab);
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUrlUpdatedNotCrashing_ForTabNotInCurrentModel() throws Exception {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(1, 1, null);
        enterGTSWithThumbnailChecking();

        Tab tab = cta.getTabModelSelector().getCurrentTab();
        switchTabModel(cta, false);

        mActivityTestRule.loadUrlInTab(
                mUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab);
    }

    private int getTabCountInCurrentTabModel() {
        return mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true"})
    public void testTabSuggestionMessageCard_dismiss() throws InterruptedException {
        prepareTabs(3, 0, null);

        // TODO(meiliang): Avoid using static variable for tracking state,
        // TabSuggestionMessageService.isSuggestionAvailableForTesting(). Instead, we can add a
        // dummy MessageObserver to track the availability of the suggestions.
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(Criteria.equals(3, this::getTabCountInCurrentTabModel));

        enterGTSWithThumbnailChecking();

        // TODO(meiliang): Avoid using static variable for tracking state,
        // TabSwitcherCoordinator::hasAppendedMessagesForTesting. Instead, we can query the number
        // of items that the inner model of the TabSwitcher has.
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.close_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true"})
    public void testTabSuggestionMessageCard_review() throws InterruptedException {
        prepareTabs(3, 0, null);

        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(Criteria.equals(3, this::getTabCountInCurrentTabModel));

        enterGTSWithThumbnailChecking();

        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());

        TabSelectionEditorTestingRobot tabSelectionEditorTestingRobot =
                new TabSelectionEditorTestingRobot();
        tabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsVisible();

        Espresso.pressBack();
        tabSelectionEditorTestingRobot.resultRobot.verifyTabSelectionEditorIsHidden();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/cleanup-delay/10000/baseline_tab_suggestions/true"})
    public void testShowOnlyOneTabSuggestionMessageCard_withSoftCleanup()
            throws InterruptedException {
        verifyOnlyOneTabSuggestionMessageCardIsShowing();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true"})
    public void testShowOnlyOneTabSuggestionMessageCard_withHardCleanup()
            throws InterruptedException {
        verifyOnlyOneTabSuggestionMessageCardIsShowing();
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study"})
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true"})
    public void testTabSuggestionMessageCardDismissAfterTabClosing() throws InterruptedException {
        prepareTabs(3, 0, mUrl);
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(Criteria.equals(3, this::getTabCountInCurrentTabModel));

        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        closeFirstTabInTabSwitcher();

        CriteriaHelper.pollUiThread(
                () -> !TabSuggestionMessageService.isSuggestionAvailableForTesting());
        CriteriaHelper.pollUiThread(Criteria.equals(2, this::getTabCountInCurrentTabModel));

        onView(withId(R.id.tab_list_view))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabSuggestionMessageCardCount(
                        0));
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature("NewTabTile")
    // clang-format off
    @DisableFeatures({ChromeFeatureList.CLOSE_TAB_SUGGESTIONS})
    @CommandLineFlags.Add({BASE_PARAMS + "/tab_grid_layout_android_new_tab_tile/NewTabTile"
            + "/tab_grid_layout_android_new_tab/false"})
    public void testNewTabTile() throws InterruptedException {
        // clang-format on
        // TODO(yuezhanggg): Modify TabUiTestHelper.verifyTabSwitcherCardCount so that it can be
        // used here to verify card count. Right now it doesn't work because when switching between
        // normal/incognito, the tab list fading-in animation has not finished when check happens.
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(2, 0, null);

        // New tab tile should be showing.
        enterGTSWithThumbnailChecking();
        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(3));
        onView(withId(R.id.new_tab_tile)).check(matches(isDisplayed()));
        verifyTabModelTabCount(cta, 2, 0);

        // Clicking new tab tile in normal mode should create a normal tab.
        onView(withId(R.id.new_tab_tile)).perform(click());
        CriteriaHelper.pollUiThread(() -> !cta.getOverviewModeBehavior().overviewVisible());
        enterGTSWithThumbnailChecking();
        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(4));
        onView(withId(R.id.new_tab_tile)).check(matches(isDisplayed()));
        verifyTabModelTabCount(cta, 3, 0);

        // New tab tile should be showing in incognito mode.
        switchTabModel(cta, true);
        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.new_tab_tile)).check(matches(isDisplayed()));

        // Clicking new tab tile in incognito mode should create an incognito tab.
        onView(withId(R.id.new_tab_tile)).perform(click());
        CriteriaHelper.pollUiThread(() -> !cta.getOverviewModeBehavior().overviewVisible());
        enterGTSWithThumbnailChecking();
        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(2));
        onView(withId(R.id.new_tab_tile)).check(matches(isDisplayed()));
        verifyTabModelTabCount(cta, 3, 1);

        // Close all normal tabs and incognito tabs, the new tab tile should still show in both
        // modes.
        switchTabModel(cta, false);
        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(4));
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.close_all_tabs_menu_id);
        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.new_tab_tile)).check(matches(isDisplayed()));
        switchTabModel(cta, true);
        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.new_tab_tile)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature("TabSuggestion")
    @EnableFeatures(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true"})
    public void testTabSuggestionMessageCard_orientation() throws InterruptedException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(3, 0, null);

        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(Criteria.equals(3, this::getTabCountInCurrentTabModel));

        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);

        // Force portrait mode since the device can be wrongly in landscape. See crbug/1063639.
        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_PORTRAIT);

        onView(withId(R.id.tab_list_view))
                .check(MessageCardWidthAssertion.checkMessageItemSpanSize(3, 2));

        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        onView(withId(R.id.tab_list_view))
                .check(MessageCardWidthAssertion.checkMessageItemSpanSize(3, 3));
    }

    private static class MessageCardWidthAssertion implements ViewAssertion {
        private int mIndex;
        private int mSpanCount;

        public static MessageCardWidthAssertion checkMessageItemSpanSize(int index, int spanCount) {
            return new MessageCardWidthAssertion(index, spanCount);
        }

        public MessageCardWidthAssertion(int index, int spanCount) {
            mIndex = index;
            mSpanCount = spanCount;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;
            int tabListPadding =
                    (int) view.getResources().getDimension(R.dimen.tab_list_card_padding);

            assertTrue(view instanceof RecyclerView);
            RecyclerView recyclerView = (RecyclerView) view;
            GridLayoutManager layoutManager = (GridLayoutManager) recyclerView.getLayoutManager();
            assertEquals(mSpanCount, layoutManager.getSpanCount());

            RecyclerView.ViewHolder messageItemViewHolder =
                    recyclerView.findViewHolderForAdapterPosition(mIndex);
            assertNotNull(messageItemViewHolder);
            assertEquals(TabProperties.UiType.MESSAGE, messageItemViewHolder.getItemViewType());
            View messageItemView = messageItemViewHolder.itemView;

            // The message card item width should always be recyclerView width minus padding.
            assertEquals(recyclerView.getWidth() - 2 * tabListPadding, messageItemView.getWidth());
        }
    }

    @Test
    @MediumTest
    @Feature("NewTabTile")
    // clang-format off
    @DisableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study",
            ChromeFeatureList.CLOSE_TAB_SUGGESTIONS})
    @CommandLineFlags.Add({BASE_PARAMS + "/tab_grid_layout_android_new_tab_tile/false"
            + "/tab_grid_layout_android_new_tab/false"})
    public void testNewTabTile_Disabled() throws InterruptedException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(2, 0, null);

        enterGTSWithThumbnailChecking();

        onView(withId(R.id.new_tab_tile)).check(doesNotExist());
        verifyTabSwitcherCardCount(cta, 2);

        switchTabModel(cta, true);
        onView(withId(R.id.new_tab_tile)).check(doesNotExist());
        verifyTabSwitcherCardCount(cta, 0);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testThumbnailAspectRatio_default() {
        prepareTabs(2, 0, mUrl);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onView(withId(R.id.tab_list_view))
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(1.0));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/thumbnail_aspect_ratio/0.75"})
    public void testThumbnailAspectRatio_point75() {
        prepareTabs(2, 0, mUrl);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onView(withId(R.id.tab_list_view))
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(0.75));
        leaveGTSAndVerifyThumbnailsAreReleased();

        Tab tab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        mActivityTestRule.loadUrlInTab(
                NTP_URL, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onView(withId(R.id.tab_list_view))
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(0.75));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/thumbnail_aspect_ratio/2.0/allow_to_refetch/true"})
    public void testThumbnailAspectRatio_fromTwoToPoint75() throws Exception {
        prepareTabs(2, 0, mUrl);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onView(withId(R.id.tab_list_view))
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(2.0));
        TabModel currentTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            TabUiTestHelper.checkThumbnailsExist(currentTabModel.getTabAt(i));
        }
        leaveGTSAndVerifyThumbnailsAreReleased();

        simulateAspectRatioChangedToPoint75();
        verifyAllThumbnailHasAspectRatio(0.75);

        enterTabSwitcher(mActivityTestRule.getActivity());
        onView(withId(R.id.tab_list_view))
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(2.0));
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testThumbnailFetchingResult_defaultAspectRatio() throws Exception {
        prepareTabs(2, 0, mUrl);
        int oldJpegCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_JPEG);
        int oldEtc1Count = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_ETC1);
        int oldNothingCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_NOTHING);
        int oldDifferentAspectRatioJpegCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG);

        enterTabSwitcher(mActivityTestRule.getActivity());
        int currentJpegCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_JPEG);
        int currentEtc1Count = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_ETC1);
        int currentNothingCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_NOTHING);
        int currentDifferentAspectRatioJpegCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG);

        assertEquals(1, currentJpegCount - oldJpegCount);
        assertEquals(0, currentEtc1Count - oldEtc1Count);
        assertEquals(0, currentDifferentAspectRatioJpegCount - oldDifferentAspectRatioJpegCount);
        // Get thumbnail from a live layer.
        assertEquals(1, currentNothingCount - oldNothingCount);

        oldJpegCount = currentJpegCount;
        oldEtc1Count = currentEtc1Count;
        oldNothingCount = currentNothingCount;
        oldDifferentAspectRatioJpegCount = currentDifferentAspectRatioJpegCount;

        TabModel currentTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            TabUiTestHelper.checkThumbnailsExist(currentTabModel.getTabAt(i));
        }

        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();

        enterTabSwitcher(mActivityTestRule.getActivity());
        assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_JPEG)
                        - oldJpegCount);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_ETC1)
                        - oldEtc1Count);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_NOTHING)
                        - oldNothingCount);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG)
                        - oldDifferentAspectRatioJpegCount);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS + "/thumbnail_aspect_ratio/2.0/allow_to_refetch/true"})
    public void testThumbnailFetchingResult_changingAspectRatio() throws Exception {
        prepareTabs(2, 0, mUrl);
        int oldJpegCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_JPEG);
        int oldEtc1Count = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_ETC1);
        int oldNothingCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_NOTHING);
        int oldDifferentAspectRatioJpegCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG);

        enterTabSwitcher(mActivityTestRule.getActivity());
        int currentJpegCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_JPEG);
        int currentEtc1Count = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_ETC1);
        int currentNothingCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_NOTHING);
        int currentDifferentAspectRatioJpegCount = RecordHistogram.getHistogramValueCountForTesting(
                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                TabContentManager.ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG);

        assertEquals(1, currentJpegCount - oldJpegCount);
        assertEquals(0, currentEtc1Count - oldEtc1Count);
        assertEquals(0, currentDifferentAspectRatioJpegCount - oldDifferentAspectRatioJpegCount);
        // Get thumbnail from a live layer.
        assertEquals(1, currentNothingCount - oldNothingCount);

        oldJpegCount = currentJpegCount;
        oldEtc1Count = currentEtc1Count;
        oldNothingCount = currentNothingCount;
        oldDifferentAspectRatioJpegCount = currentDifferentAspectRatioJpegCount;

        onView(withId(R.id.tab_list_view))
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(2.0));

        TabModel currentTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            TabUiTestHelper.checkThumbnailsExist(currentTabModel.getTabAt(i));
        }
        leaveGTSAndVerifyThumbnailsAreReleased();

        simulateAspectRatioChangedToPoint75();
        verifyAllThumbnailHasAspectRatio(0.75);

        enterTabSwitcher(mActivityTestRule.getActivity());
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_JPEG)
                        - oldJpegCount);
        assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_DIFFERENT_ASPECT_RATIO_JPEG)
                        - oldDifferentAspectRatioJpegCount);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_ETC1)
                        - oldEtc1Count);
        assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                        TabContentManager.ThumbnailFetchingResult.GOT_NOTHING)
                        - oldNothingCount);
        onView(withId(R.id.tab_list_view))
                .check(ThumbnailAspectRatioAssertion.havingAspectRatio(2.0));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    public void testRecycling_defaultAspectRatio() {
        prepareTabs(10, 0, mUrl);
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onView(withId(R.id.tab_list_view)).perform(RecyclerViewActions.scrollToPosition(9));
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/thumbnail_aspect_ratio/0.75"})
    public void testRecycling_aspectRatioPoint75() {
        // clang-format on
        prepareTabs(10, 0, mUrl);
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onView(withId(R.id.tab_list_view)).perform(RecyclerViewActions.scrollToPosition(9));
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS + "/thumbnail_aspect_ratio/0.75"})
    public void testExpandTab_withAspectRatioPoint75() {
        // clang-format on
        prepareTabs(1, 0, mUrl);
        enterTabSwitcher(mActivityTestRule.getActivity());
        leaveGTSAndVerifyThumbnailsAreReleased();
    }

    @Test
    @MediumTest
    @Feature("NewTabVariation")
    // clang-format off
    @Features.DisableFeatures({ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study",
            ChromeFeatureList.CLOSE_TAB_SUGGESTIONS})
    @CommandLineFlags.Add({BASE_PARAMS + "/tab_grid_layout_android_new_tab/NewTabVariation"})
    public void testNewTabVariation() {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(2, 0, null);
        enterTabSwitcher(cta);
        verifyTabModelTabCount(cta, 2, 0);
        checkNewTabVariationVisibility(true);

        createTabs(cta, true, 1);
        verifyTabModelTabCount(cta, 2, 1);
        enterTabSwitcher(cta);
        checkNewTabVariationVisibility(false);

        switchTabModel(cta, false);
        checkNewTabVariationVisibility(false);

        switchTabModel(cta, true);
        checkNewTabVariationVisibility(false);

        closeFirstTabInTabSwitcher();
        verifyTabModelTabCount(cta, 2, 0);
        checkNewTabVariationVisibility(true);

        createTabs(cta, true, 2);
        verifyTabModelTabCount(cta, 2, 2);
        enterTabSwitcher(cta);
        checkNewTabVariationVisibility(false);

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), cta,
                R.id.close_all_incognito_tabs_menu_id);
        verifyTabModelTabCount(cta, 2, 0);
        checkNewTabVariationVisibility(true);
    }

    private void checkNewTabVariationVisibility(boolean isVisible) {
        if (isVisible) {
            onView(allOf(withId(R.id.incognito_toggle_tabs),
                           withParent(withId(R.id.tab_switcher_toolbar))))
                    .check(matches(withEffectiveVisibility(GONE)));
            onView(allOf(withId(R.id.new_tab_button),
                           withParent(withId(R.id.tab_switcher_toolbar))))
                    .check(matches(withEffectiveVisibility(GONE)));
            onView(allOf(withId(R.id.new_tab_view), withParent(withId(R.id.tab_switcher_toolbar))))
                    .check(matches(isDisplayed()));
        } else {
            onView(allOf(withId(R.id.incognito_toggle_tabs),
                           withParent(withId(R.id.tab_switcher_toolbar))))
                    .check(matches(isDisplayed()));
            onView(allOf(withId(R.id.new_tab_button),
                           withParent(withId(R.id.tab_switcher_toolbar))))
                    .check(matches(isDisplayed()));
            onView(allOf(withId(R.id.new_tab_view), withParent(withId(R.id.tab_switcher_toolbar))))
                    .check(matches(withEffectiveVisibility(GONE)));
        }
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCloseTabViaCloseButton() throws Exception {
        // clang-format on
        mActivityTestRule.getActivity().getSnackbarManager().disableForTesting();
        prepareTabs(1, 0, null);
        enterGTSWithThumbnailChecking();

        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.content_view))))
                .perform(click());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({BASE_PARAMS})
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testSwipeToDismiss_GTS() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Create 3 tabs and merge the first two tabs into one group.
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup = new ArrayList<>(
                Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 2);
        verifyTabModelTabCount(cta, 3, 0);

        // Swipe to dismiss a single tab card.
        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        1, getSwipeToDismissAction(false)));
        verifyTabSwitcherCardCount(cta, 1);
        verifyTabModelTabCount(cta, 2, 0);

        // Swipe to dismiss a tab group card.
        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        0, getSwipeToDismissAction(true)));
        verifyTabSwitcherCardCount(cta, 0);
        verifyTabModelTabCount(cta, 0, 0);
    }

    private static class TabCountAssertion implements ViewAssertion {
        private int mExpectedCount;

        public static TabCountAssertion havingTabCount(int tabCount) {
            return new TabCountAssertion(tabCount);
        }

        public TabCountAssertion(int expectedCount) {
            mExpectedCount = expectedCount;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            RecyclerView.Adapter adapter = ((RecyclerView) view).getAdapter();
            assertEquals(mExpectedCount, adapter.getItemCount());
        }
    }

    private static class ThumbnailAspectRatioAssertion implements ViewAssertion {
        private double mExpectedRatio;

        public static ThumbnailAspectRatioAssertion havingAspectRatio(double ratio) {
            return new ThumbnailAspectRatioAssertion(ratio);
        }

        private ThumbnailAspectRatioAssertion(double expectedRatio) {
            mExpectedRatio = expectedRatio;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            RecyclerView recyclerView = (RecyclerView) view;

            RecyclerView.Adapter adapter = recyclerView.getAdapter();
            for (int i = 0; i < adapter.getItemCount(); i++) {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(i);
                if (viewHolder != null) {
                    ViewLookupCachingFrameLayout tabView =
                            (ViewLookupCachingFrameLayout) viewHolder.itemView;
                    ImageView thumbnail = (ImageView) tabView.fastFindViewById(R.id.tab_thumbnail);
                    BitmapDrawable drawable = (BitmapDrawable) thumbnail.getDrawable();
                    Bitmap bitmap = drawable.getBitmap();
                    double bitmapRatio = bitmap.getWidth() * 1.0 / bitmap.getHeight();
                    assertTrue(
                            "Actual ratio: " + bitmapRatio + "; Expected ratio: " + mExpectedRatio,
                            Math.abs(bitmapRatio - mExpectedRatio)
                                    <= TabContentManager.ASPECT_RATIO_PRECISION);
                }
            }
        }
    }

    private void waitForLastSearchTerm(Tab tab, String expected) {
        CriteriaHelper.pollUiThread(
                Criteria.equals(expected, () -> TabAttributeCache.getLastSearchTerm(tab.getId())));
    }

    @Test
    @MediumTest
    // Disable TAB_TO_GTS_ANIMATION to make it less flaky.
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/enable_search_term_chip/true"})
    public void testSearchTermChip_noChip() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.ENABLE_SEARCH_CHIP.getValue());
        prepareTabs(1, 0, mUrl);
        enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.search_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    // Disable TAB_TO_GTS_ANIMATION to make it less flaky.
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/enable_search_term_chip/true"})
    public void testSearchTermChip_withChip() throws InterruptedException {
        assertTrue(TabUiFeatureUtilities.ENABLE_SEARCH_CHIP.getValue());
        // Make sure we support RTL and CJKV languages.
        String searchTermWithSpecialCodePoints = "a\n ئۇيغۇرچە\u200E漢字";
        // Special code points like new line (\n) and left-to-right marker (‎‎‎\u200E) should
        // be stripped out. See TabAttributeCache#removeEscapedCodePoints for more details.
        String expectedTerm = "a ئۇيغۇرچە漢字";

        String anotherTerm = "hello world";

        // Do search, and verify the chip is still not shown.
        AtomicReference<String> searchUrl = new AtomicReference<>();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Tab currentTab = cta.getTabModelSelector().getCurrentTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TemplateUrlServiceFactory.get().setSearchEngine("google.com");
            searchUrl.set(TemplateUrlServiceFactory.get().getUrlForSearchQuery(
                    searchTermWithSpecialCodePoints));
            currentTab.loadUrl(new LoadUrlParams(searchUrl.get()));
        });
        ChromeTabUtils.waitForTabPageLoaded(currentTab, null);
        enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.search_button)).check(matches(not(isDisplayed())));
        Espresso.pressBack();

        // Navigate, and verify the chip is shown.
        mActivityTestRule.loadUrl(mUrl);
        waitForLastSearchTerm(currentTab, expectedTerm);
        enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.search_button))
                .check(waitForView(allOf(withText(expectedTerm), isDisplayed())));
        Espresso.pressBack();

        // Do another search, and verify the chip is gone.
        AtomicReference<String> searchUrl2 = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TemplateUrlServiceFactory.get().setSearchEngine("google.com");
            searchUrl2.set(TemplateUrlServiceFactory.get().getUrlForSearchQuery(anotherTerm));
            currentTab.loadUrl(new LoadUrlParams(searchUrl2.get()));
        });
        ChromeTabUtils.waitForTabPageLoaded(currentTab, null);
        waitForLastSearchTerm(currentTab, null);
        enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.search_button)).check(matches(not(isDisplayed())));
        Espresso.pressBack();

        // Back to previous page, and verify the chip is back.
        Espresso.pressBack();
        waitForLastSearchTerm(currentTab, expectedTerm);
        enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.search_button))
                .check(waitForView(allOf(withText(expectedTerm), isDisplayed())));

        // Click the chip and check the tab navigates back to the search result page.
        assertEquals(mUrl, currentTab.getUrlString());
        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        onView(withId(R.id.search_button))
                .check(waitForView(allOf(withText(expectedTerm), isDisplayed())));
        onView(withId(R.id.search_button)).perform(click());
        hideWatcher.waitForBehavior();
        ChromeTabUtils.waitForTabPageLoaded(currentTab, searchUrl.get());

        // Verify the chip is gone.
        waitForLastSearchTerm(currentTab, null);
        enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.search_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    // clang-format off
    // Disable TAB_TO_GTS_ANIMATION to make it less flaky.
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS +
            "/enable_search_term_chip/true/enable_search_term_chip_adaptive_icon/true"})
    public void testSearchTermChip_adaptiveIcon() throws InterruptedException {
        // clang-format on
        assertTrue(TabUiFeatureUtilities.ENABLE_SEARCH_CHIP.getValue());
        assertTrue(TabUiFeatureUtilities.ENABLE_SEARCH_CHIP_ADAPTIVE.getValue());
        String searchTerm = "hello world";

        // Do search, and verify the chip is still not shown.
        AtomicReference<String> searchUrl = new AtomicReference<>();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Tab currentTab = cta.getTabModelSelector().getCurrentTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TemplateUrlServiceFactory.get().setSearchEngine("google.com");
            searchUrl.set(TemplateUrlServiceFactory.get().getUrlForSearchQuery(searchTerm));
            currentTab.loadUrl(new LoadUrlParams(searchUrl.get()));
        });
        ChromeTabUtils.waitForTabPageLoaded(currentTab, null);
        enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.search_button)).check(matches(not(isDisplayed())));
        Espresso.pressBack();

        // Navigate, and verify the chip is shown.
        mActivityTestRule.loadUrl(mUrl);
        waitForLastSearchTerm(currentTab, searchTerm);
        enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.tab_list_view)).check(TabCountAssertion.havingTabCount(1));
        onView(withId(R.id.search_button))
                .check(waitForView(allOf(withText(searchTerm), isDisplayed())));

        // Switch the default search engine from google.com to yahoo.com, the search chip icon
        // should change.
        RecyclerView tabListRecyclerView = cta.findViewById(R.id.tab_list_view);
        ChipView chipView =
                tabListRecyclerView.findViewHolderForAdapterPosition(0).itemView.findViewById(
                        R.id.search_button);
        ChromeImageView iconImageView = (ChromeImageView) chipView.getChildAt(0);
        Drawable googleDrawable = iconImageView.getDrawable();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> TemplateUrlServiceFactory.get().setSearchEngine("yahoo.com"));

        assertNotEquals(googleDrawable, iconImageView.getDrawable());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    public void testTabGroupManualSelection() throws InterruptedException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .check(TabCountAssertion.havingTabCount(3));

        enterTabGroupManualSelection(cta);
        robot.resultRobot.verifyTabSelectionEditorIsVisible();

        // Group first two tabs.
        robot.actionRobot.clickItemAtAdapterPosition(0);
        robot.actionRobot.clickItemAtAdapterPosition(1);
        robot.actionRobot.clickToolbarActionButton();

        // Exit manual selection mode, back to tab switcher.
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .check(TabCountAssertion.havingTabCount(2));
        onViewWaiting(withText("2 tabs grouped"));
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.CLOSE_TAB_SUGGESTIONS + "<Study"})
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    @CommandLineFlags.Add({BASE_PARAMS + "/baseline_tab_suggestions/true"})
    public void testTabGroupManualSelection_AfterReviewTabSuggestion() throws InterruptedException {
        // clang-format on
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TabSelectionEditorTestingRobot robot = new TabSelectionEditorTestingRobot();
        createTabs(cta, false, 3);

        // Review closing tab suggestion.
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(Criteria.equals(3, this::getTabCountInCurrentTabModel));

        // Entering GTS with thumbnail checking here is trying to reduce flakiness that is caused by
        // the TabContextObserver. TabContextObserver listens to
        // TabObserver#didFirstVisuallyNonEmptyPaint and invalidates the suggestion. Do the
        // thumbnail checking here is to ensure the suggestion is valid when entering tab switcher.
        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());

        robot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonWithResourceId(
                        R.string.tab_suggestion_close_tab_action_button);

        robot.actionRobot.clickToolbarActionButton();
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
        CriteriaHelper.pollUiThread(
                Criteria.equals(0, mActivityTestRule.getActivity().getCurrentTabModel()::getCount));

        // Show Manual Selection Mode.
        createTabs(cta, false, 3);

        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        enterTabGroupManualSelection(cta);
        robot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonWithResourceId(R.string.tab_selection_editor_group);

        // Group first two tabs.
        robot.actionRobot.clickItemAtAdapterPosition(0);
        robot.actionRobot.clickItemAtAdapterPosition(1);
        robot.actionRobot.clickToolbarActionButton();

        // Exit manual selection mode, back to tab switcher.
        robot.resultRobot.verifyTabSelectionEditorIsHidden();
        onViewWaiting(withText("2 tabs grouped"));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = N_MR1, message = "crbug.com/1077191, crbug.com/1081909")
    public void testActivityCanBeGarbageCollectedAfterFinished() throws Exception {
        prepareTabs(1, 0, "about:blank");

        WeakReference<ChromeTabbedActivity> activityRef =
                new WeakReference<>(mActivityTestRule.getActivity());

        ChromeTabbedActivity activity =
                ApplicationTestUtils.recreateActivity(mActivityTestRule.getActivity());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mStartSurfaceLayout = null;
        mTabListDelegate = null;
        mActivityTestRule.setActivity(activity);

        assertTrue(GarbageCollectionTestUtils.canBeGarbageCollected(activityRef));
    }

    /**
     * This test is to simulate closing all incognito tabs during the activity recreation stage.
     * This is a regression test for crbug.com/1044557. The test ensures all incognito tabs closed,
     * which leads to a TabModel switching event, before the first normal tab is restored.
     */
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION + "<Study")
    // clang-format off
    @DisabledTest(message = "crbug.com/1044557 This regression test fails deterministically with" +
            " the bot, but passes with local emulator. Disable the test for now.")
    public void
    closeAllIncognitoTabsBeforeRestoreCompleted() {
        // clang-format on
        prepareTabs(3, 1, "about:blank");

        assertTrue(mActivityTestRule.getActivity().getCurrentTabModel().isIncognito());

        // Need to wait for contentsState to be initialized for the tab to restore correctly.
        CriteriaHelper.pollUiThread(
                ()
                        -> TabState.from(mActivityTestRule.getActivity().getActivityTab())
                                   .contentsState
                        != null,
                "Incognito tab contentsState is null");

        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), true, false);

        mActivityTestRule.loadUrl("about:blank");

        // Need to wait for contentsState to be initialized for the tab to restore correctly.
        CriteriaHelper.pollUiThread(
                ()
                        -> TabState.from(mActivityTestRule.getActivity().getActivityTab())
                                   .contentsState
                        != null,
                "Incognito tab contentsState is null");

        PseudoTab.clearForTesting();

        AtomicBoolean tabModelRestoreCompleted = new AtomicBoolean(false);
        AtomicBoolean normalModelIsEmpty = new AtomicBoolean(true);

        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.recreateActivity(mActivityTestRule.getActivity());

        new TabModelSelectorTabModelObserver(newActivity.getTabModelSelector()) {
            @Override
            public void restoreCompleted() {
                tabModelRestoreCompleted.set(true);
            }

            @Override
            public void willAddTab(Tab tab, int type) {
                if (tab.isIncognito() || !normalModelIsEmpty.get()) return;

                // Ensure normal tab model is active before its first tab restores.
                CriteriaHelper.pollUiThread(
                        ()
                                -> !newActivity.getCurrentTabModel().isIncognito(),
                        "Had not switched to Normal TabModel before the first normal Tab restored");
            }

            @Override
            public void didAddTab(Tab tab, int type, int creationState) {
                if (tab.isIncognito()) return;
                normalModelIsEmpty.set(false);
            }
        };

        CriteriaHelper.pollUiThread(()
                                            -> newActivity.didFinishNativeInitialization(),
                "New Activity has not finished initialized yet");
        CriteriaHelper.pollUiThread(()
                                            -> newActivity.getCurrentTabModel().isIncognito(),
                "New Activity current TabModel is not incognito");
        TestThreadUtils.runOnUiThreadBlocking(IncognitoUtils::closeAllIncognitoTabs);

        CriteriaHelper.pollUiThread(
                ()
                        -> DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp(),
                "Deferred startup never completed");

        CriteriaHelper.pollUiThread(()
                                            -> tabModelRestoreCompleted.get(),
                "New Activity normal TabModel has never restored");

        assertFalse(newActivity.getCurrentTabModel().isIncognito());
        assertNotNull(newActivity.getTabModelSelector().getCurrentTab());
        // With the fix, StaticLayout guarantees to be shown. Otherwise, StartSurfaceLayout could be
        // shown, not guaranteed.
        assertTrue(newActivity.getLayoutManager().getActiveLayout() instanceof StaticLayout);
    }

    private void enterTabGroupManualSelection(ChromeTabbedActivity cta) {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.menu_group_tabs);
    }

    /**
     * TODO(wychen): move some of the callers to {@link TabUiTestHelper#enterTabSwitcher}.
     */
    private void enterGTSWithThumbnailChecking() throws InterruptedException {
        Tab currentTab = mActivityTestRule.getActivity().getTabModelSelector().getCurrentTab();
        // Native tabs need to be invalidated first to trigger thumbnail taking, so skip them.
        boolean checkThumbnail = !currentTab.isNativePage();

        if (checkThumbnail) {
            mActivityTestRule.getActivity().getTabContentManager().removeTabThumbnail(
                    currentTab.getId());
        }

        int count = getCaptureCount();
        waitForCaptureRateControl();
        // TODO(wychen): use TabUiTestHelper.enterTabSwitcher() instead.
        //  Might increase flakiness though. See crbug.com/1024742.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(true));
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        // Make sure the fading animation is done.
        int delta;
        if (TextUtils.equals(
                    mActivityTestRule.getActivity().getCurrentWebContents().getLastCommittedUrl(),
                    NTP_URL)) {
            // NTP is not invalidated, so no new captures.
            delta = 0;
        } else {
            // The final capture at StartSurfaceLayout#finishedShowing time.
            delta = 1;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                    && areAnimatorsEnabled()) {
                // The faster capturing without writing back to cache.
                delta += 1;
            }
        }
        checkCaptureCount(delta, count);
        TabUiTestHelper.verifyAllTabsHaveThumbnail(
                mActivityTestRule.getActivity().getCurrentTabModel());
    }

    /**
     * Like {@link TabUiTestHelper#enterTabSwitcher}, but make sure all tabs have thumbnail.
     */
    private void enterGTSWithThumbnailRetry() {
        enterTabSwitcher(mActivityTestRule.getActivity());
        try {
            TabUiTestHelper.verifyAllTabsHaveThumbnail(
                    mActivityTestRule.getActivity().getCurrentTabModel());
        } catch (AssertionError ae) {
            // If the last thumbnail is missing, try without animation.
            Espresso.pressBack();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));
            TabUiTestHelper.verifyAllTabsHaveThumbnail(
                    mActivityTestRule.getActivity().getCurrentTabModel());
        }
    }

    /**
     * If thumbnail checking is not needed, use {@link TabUiTestHelper#leaveTabSwitcher} instead.
     */
    private void leaveGTSAndVerifyThumbnailsAreReleased() {
        assertTrue(mActivityTestRule.getActivity().getLayoutManager().overviewVisible());

        StartSurface startSurface = mStartSurfaceLayout.getStartSurfaceForTesting();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { startSurface.getController().onBackPressed(); });
        // TODO(wychen): using default timeout or even converting to
        //  OverviewModeBehaviorWatcher shouldn't increase flakiness.
        CriteriaHelper.pollUiThread(
                ()
                        -> !mActivityTestRule.getActivity().getLayoutManager().overviewVisible(),
                "Overview not hidden yet", DEFAULT_MAX_TIME_TO_POLL * 10, DEFAULT_POLLING_INTERVAL);
        assertThumbnailsAreReleased();
    }

    private void checkFinalCaptureCount(boolean switchToAnotherTab, int initCount) {
        int expected;
        if (TextUtils.equals(
                    mActivityTestRule.getActivity().getCurrentWebContents().getLastCommittedUrl(),
                    NTP_URL)) {
            expected = 0;
        } else {
            expected = mRepeat;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
                    && areAnimatorsEnabled()) {
                expected += mRepeat;
            }
            if (switchToAnotherTab) {
                expected += mRepeat;
            }
        }
        checkCaptureCount(expected, initCount);
    }

    private void checkCaptureCount(int expectedDelta, int initCount) {
        // TODO(wychen): With animation, the 2nd capture might be skipped if the 1st takes too long.
        CriteriaHelper.pollUiThread(
                Criteria.equals(expectedDelta, () -> getCaptureCount() - initCount));
    }

    private int getCaptureCount() {
        return RecordHistogram.getHistogramTotalCountForTesting("Compositing.CopyFromSurfaceTime");
    }

    private void waitForCaptureRateControl() throws InterruptedException {
        // Needs to wait for at least |kCaptureMinRequestTimeMs| in order to capture another one.
        // TODO(wychen): find out why waiting is still needed after setting
        //               |kCaptureMinRequestTimeMs| to 0.
        Thread.sleep(2000);
    }

    private void assertThumbnailsAreReleased() {
        // Could not directly assert canAllBeGarbageCollected() because objects can be in Cleaner.
        CriteriaHelper.pollUiThread(() -> canAllBeGarbageCollected(mAllBitmaps));
    }

    private boolean canAllBeGarbageCollected(List<WeakReference<Bitmap>> bitmaps) {
        for (WeakReference<Bitmap> bitmap : bitmaps) {
            if (!GarbageCollectionTestUtils.canBeGarbageCollected(bitmap)) {
                return false;
            }
        }
        return true;
    }

    private void simulateAspectRatioChangedToPoint75() throws IOException {
        TabModel currentModel = mActivityTestRule.getActivity().getCurrentTabModel();
        for (int i = 0; i < currentModel.getCount(); i++) {
            Tab tab = currentModel.getTabAt(i);
            Bitmap bitmap = TabContentManager.getJpegForTab(tab.getId());
            bitmap = Bitmap.createScaledBitmap(
                    bitmap, bitmap.getWidth(), (int) (bitmap.getWidth() * 1.0 / 0.75), false);
            encodeJpeg(tab, bitmap);
        }
    }

    private void encodeJpeg(Tab tab, Bitmap bitmap) throws IOException {
        FileOutputStream outputStream =
                new FileOutputStream(TabContentManager.getTabThumbnailFileJpeg(tab.getId()));
        bitmap.compress(Bitmap.CompressFormat.JPEG, 50, outputStream);
        outputStream.close();
    }

    private void verifyAllThumbnailHasAspectRatio(double ratio) {
        TabModel currentModel = mActivityTestRule.getActivity().getCurrentTabModel();
        for (int i = 0; i < currentModel.getCount(); i++) {
            Tab tab = currentModel.getTabAt(i);
            Bitmap bitmap = TabContentManager.getJpegForTab(tab.getId());
            double bitmapRatio = bitmap.getWidth() * 1.0 / bitmap.getHeight();
            assertTrue("Actual ratio: " + bitmapRatio + "; Expected ratio: " + ratio,
                    Math.abs(bitmapRatio - ratio) <= TabContentManager.ASPECT_RATIO_PRECISION);
        }
    }

    private void verifyOnlyOneTabSuggestionMessageCardIsShowing() throws InterruptedException {
        String suggestionMessageTemplate = mActivityTestRule.getActivity().getString(
                org.chromium.chrome.tab_ui.R.string.tab_suggestion_close_stale_message);
        String suggestionMessage =
                String.format(Locale.getDefault(), suggestionMessageTemplate, "3");
        prepareTabs(3, 0, mUrl);
        CriteriaHelper.pollUiThread(TabSuggestionMessageService::isSuggestionAvailableForTesting);
        CriteriaHelper.pollUiThread(Criteria.equals(3, this::getTabCountInCurrentTabModel));

        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(allOf(withText(suggestionMessage), withParent(withId(R.id.tab_grid_message_item))))
                .check(matches(isDisplayed()));
        leaveGTSAndVerifyThumbnailsAreReleased();

        // With soft or hard clean up depends on the soft-cleanup-delay and cleanup-delay params.
        enterGTSWithThumbnailChecking();
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        // This will fail with error "matched multiple views" when there is more than one suggestion
        // message card.
        onView(allOf(withText(suggestionMessage), withParent(withId(R.id.tab_grid_message_item))))
                .check(matches(isDisplayed()));
    }
}
