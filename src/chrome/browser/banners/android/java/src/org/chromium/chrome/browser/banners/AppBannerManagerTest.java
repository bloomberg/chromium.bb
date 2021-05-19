// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiSelector;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.RootMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.intents.BitmapHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.browser.webapps.PwaInstallBottomSheetView;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.CppWrappedTestTracker;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarAnimationListener;
import org.chromium.components.infobars.InfoBarUiItem;
import org.chromium.components.site_engagement.SiteEngagementService;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppData;
import org.chromium.components.webapps.AppDetailsDelegate;
import org.chromium.components.webapps.installable.InstallableAmbientBadgeInfoBar;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests the app banners.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AppBannerManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    // A callback that fires when the IPH system sends an event.
    private final CallbackHelper mOnEventCallback = new CallbackHelper();

    // The ID of the last event received.
    private String mLastNotifyEvent;

    private static final String NATIVE_APP_MANIFEST_WITH_ID =
            "/chrome/test/data/banners/play_app_manifest.json";

    private static final String NATIVE_APP_MANIFEST_WITH_URL =
            "/chrome/test/data/banners/play_app_url_manifest.json";

    private static final String WEB_APP_MANIFEST_WITH_UNSUPPORTED_PLATFORM =
            "/chrome/test/data/banners/manifest_prefer_related_chrome_app.json";

    private static final String WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL =
            "/chrome/test/data/banners/manifest_bottom_sheet_install.json";

    private static final String NATIVE_ICON_PATH = "/chrome/test/data/banners/launcher-icon-4x.png";

    private static final String NATIVE_APP_TITLE = "Mock app title";

    private static final String NATIVE_APP_INSTALL_TEXT = "Install this";

    private static final String NATIVE_APP_REFERRER = "chrome_inline&playinline=chrome_inline";

    private static final String NATIVE_APP_BLANK_REFERRER = "playinline=chrome_inline";

    private static final String INSTALL_ACTION = "INSTALL_ACTION";

    private static final String INSTALL_PATH_HISTOGRAM_NAME = "WebApk.Install.PathToInstall";

    private class MockAppDetailsDelegate extends AppDetailsDelegate {
        private Observer mObserver;
        private AppData mAppData;
        private int mNumRetrieved;
        private Intent mInstallIntent;
        private String mReferrer;

        @Override
        public void getAppDetailsAsynchronously(
                Observer observer, String url, String packageName, String referrer, int iconSize) {
            mNumRetrieved += 1;
            mObserver = observer;
            mReferrer = referrer;
            mInstallIntent = new Intent(INSTALL_ACTION);

            mAppData = new AppData(url, packageName);
            mAppData.setPackageInfo(NATIVE_APP_TITLE, mTestServer.getURL(NATIVE_ICON_PATH), 4.5f,
                    NATIVE_APP_INSTALL_TEXT, null, mInstallIntent);
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                    () -> { mObserver.onAppDetailsRetrieved(mAppData); });
        }

        @Override
        public void destroy() {}
    }

    private static class TestDataStorageFactory extends WebappDataStorage.Factory {
        public String mSplashImage;

        @Override
        public WebappDataStorage create(final String webappId) {
            return new WebappDataStorageWrapper(webappId);
        }

        private class WebappDataStorageWrapper extends WebappDataStorage {
            public WebappDataStorageWrapper(String webappId) {
                super(webappId);
            }

            @Override
            public void updateSplashScreenImage(String splashScreenImage) {
                Assert.assertNull(mSplashImage);
                mSplashImage = splashScreenImage;
            }
        }
    }

    private static class InfobarListener implements InfoBarAnimationListener {
        private boolean mDoneAnimating;

        @Override
        public void notifyAnimationFinished(int animationType) {
            if (animationType == InfoBarAnimationListener.ANIMATION_TYPE_SHOW) {
                mDoneAnimating = true;
            }
        }

        @Override
        public void notifyAllAnimationsFinished(InfoBarUiItem frontInfoBar) {
            mDoneAnimating = true;
        }
    }

    private MockAppDetailsDelegate mDetailsDelegate;
    @Mock
    private PackageManager mPackageManager;
    private EmbeddedTestServer mTestServer;
    private UiDevice mUiDevice;
    private CppWrappedTestTracker mTracker;
    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() throws Exception {
        AppBannerManager.setIsSupported(true);
        ShortcutHelper.setDelegateForTests(new ShortcutHelper.Delegate() {
            @Override
            public void addShortcutToHomescreen(String id, String title, Bitmap icon,
                    boolean iconAdaptive, Intent shortcutIntent) {
                // Ignore to prevent adding homescreen shortcuts.
            }
        });

        mTracker = new CppWrappedTestTracker(FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE) {
            @Override
            public void notifyEvent(String event) {
                super.notifyEvent(event);
                mOnEventCallback.notifyCalled();
            }
        };

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.getLastUsedRegularProfile();
            TrackerFactory.setTestingFactory(profile, mTracker);
        });

        mTabbedActivityTestRule.startMainActivityOnBlankPage();
        // Must be set after native has loaded.
        mDetailsDelegate = new MockAppDetailsDelegate();

        ThreadUtils.runOnUiThreadBlocking(
                () -> { AppBannerManager.setAppDetailsDelegate(mDetailsDelegate); });

        AppBannerManager.ignoreChromeChannelForTesting();
        AppBannerManager.setTotalEngagementForTesting(10);
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        mBottomSheetController = mTabbedActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    private void resetEngagementForUrl(final String url, final double engagement) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // TODO (https://crbug.com/1063807):  Add incognito mode tests.
            SiteEngagementService.getForBrowserContext(Profile.getLastUsedRegularProfile())
                    .resetBaseScoreForUrl(url, engagement);
        });
    }

    private AppBannerManager getAppBannerManager(WebContents webContents) {
        return AppBannerManager.forWebContents(webContents);
    }

    private void waitForBannerManager(Tab tab) {
        CriteriaHelper.pollUiThread(
                () -> !getAppBannerManager(tab.getWebContents()).isRunningForTesting());
    }

    private void navigateToUrlAndWaitForBannerManager(
            ChromeActivityTestRule<? extends ChromeActivity> rule, String url) throws Exception {
        Tab tab = rule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(url);
        waitForBannerManager(tab);
    }

    private void waitUntilAppDetailsRetrieved(
            ChromeActivityTestRule<? extends ChromeActivity> rule, final int numExpected) {
        CriteriaHelper.pollUiThread(() -> {
            AppBannerManager manager =
                    getAppBannerManager(rule.getActivity().getActivityTab().getWebContents());
            Criteria.checkThat(mDetailsDelegate.mNumRetrieved, Matchers.is(numExpected));
            Criteria.checkThat(manager.isRunningForTesting(), Matchers.is(false));
        });
    }

    private void waitUntilAmbientBadgeInfoBarAppears(
            ChromeActivityTestRule<? extends ChromeActivity> rule) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR)) {
            CriteriaHelper.pollUiThread(() -> {
                List<InfoBar> infobars = rule.getInfoBars();
                Criteria.checkThat(infobars.size(), Matchers.is(1));
                Criteria.checkThat(
                        infobars.get(0), Matchers.instanceOf(InstallableAmbientBadgeInfoBar.class));
            });
        }
    }

    private void waitUntilBottomSheetStatus(ChromeActivityTestRule<? extends ChromeActivity> rule,
            @BottomSheetController.SheetState int status) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mBottomSheetController.getSheetState(), Matchers.is(status));
        });
    }

    private static String getExpectedDialogTitle(Tab tab) throws Exception {
        String title = ThreadUtils.runOnUiThreadBlocking(() -> {
            return TabUtils.getActivity(tab).getString(
                    AppBannerManager.getHomescreenLanguageOption(tab.getWebContents()).titleTextId);
        });
        return title;
    }

    private void waitUntilNoDialogsShowing(final Tab tab) throws Exception {
        UiObject dialogUiObject =
                mUiDevice.findObject(new UiSelector().text(getExpectedDialogTitle(tab)));
        dialogUiObject.waitUntilGone(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    private void tapAndWaitForModalBanner(final Tab tab) throws Exception {
        TouchCommon.singleClickView(tab.getView());

        UiObject dialogUiObject =
                mUiDevice.findObject(new UiSelector().text(getExpectedDialogTitle(tab)));
        Assert.assertTrue(dialogUiObject.waitForExists(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL));
    }

    private void triggerModalWebAppBanner(ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url, boolean installApp) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAmbientBadgeInfoBarAppears(rule);

        Tab tab = rule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);

        if (!installApp) return;

        // Click the button to trigger the adding of the shortcut.
        clickButton(rule.getActivity(), ButtonType.POSITIVE);
    }

    private void triggerModalNativeAppBanner(ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url, String expectedReferrer, boolean installApp) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAppDetailsRetrieved(rule, 1);
        waitUntilAmbientBadgeInfoBarAppears(rule);
        Assert.assertEquals(mDetailsDelegate.mReferrer, expectedReferrer);

        final ChromeActivity activity = rule.getActivity();
        tapAndWaitForModalBanner(activity.getActivityTab());
        if (!installApp) return;

        // Click the button to trigger the installation.
        final ActivityMonitor activityMonitor =
                new ActivityMonitor(new IntentFilter(INSTALL_ACTION),
                        new ActivityResult(Activity.RESULT_OK, null), true);
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        instrumentation.addMonitor(activityMonitor);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            String buttonText = activity.getModalDialogManager().getCurrentDialogForTest().get(
                    ModalDialogProperties.POSITIVE_BUTTON_TEXT);
            Assert.assertEquals(NATIVE_APP_INSTALL_TEXT, buttonText);
        });

        clickButton(activity, ButtonType.POSITIVE);

        // Wait until the installation triggers.
        instrumentation.waitForMonitorWithTimeout(
                activityMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    private void triggerModalBannerMultipleTimes(
            ChromeActivityTestRule<? extends ChromeActivity> rule, String url,
            boolean isForNativeApp) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        if (isForNativeApp) {
            waitUntilAppDetailsRetrieved(rule, 1);
        }

        waitUntilAmbientBadgeInfoBarAppears(rule);
        Tab tab = rule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);

        // Explicitly dismiss the banner. We should be able to show the banner after dismissing.
        clickButton(rule.getActivity(), ButtonType.NEGATIVE);
        waitUntilNoDialogsShowing(tab);
        tapAndWaitForModalBanner(tab);

        clickButton(rule.getActivity(), ButtonType.NEGATIVE);
        waitUntilNoDialogsShowing(tab);
        tapAndWaitForModalBanner(tab);
    }

    private void triggerBottomSheet(ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url, boolean click) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);

        if (click) {
            final ChromeActivity activity = rule.getActivity();
            TouchCommon.singleClickView(activity.getActivityTab().getView());
            waitUntilBottomSheetStatus(rule, BottomSheetController.SheetState.FULL);
            return;
        }

        waitUntilBottomSheetStatus(rule, BottomSheetController.SheetState.PEEK);
    }

    private void clickButton(final ChromeActivity activity, @ButtonType final int buttonType) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = activity.getModalDialogManager().getCurrentDialogForTest();
            model.get(ModalDialogProperties.CONTROLLER).onClick(model, buttonType);
        });
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledEventModalWebAppBannerBrowserTab() throws Exception {
        // Sets the overridden factory to observer splash screen update.
        final TestDataStorageFactory dataStorageFactory = new TestDataStorageFactory();
        WebappDataStorage.setFactoryForTests(dataStorageFactory);

        triggerModalWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click_verify_appinstalled"),
                true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mTabbedActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.Install.InstallEvent", 4 /* API_BROWSER_TAB */));

            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            INSTALL_PATH_HISTOGRAM_NAME, /* kApiInitiateInfobar= */ 3));
        });

        // Make sure that the splash screen icon was downloaded.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(dataStorageFactory.mSplashImage, Matchers.notNullValue());
        });

        // Test that bitmap sizes match expectations.
        int idealSize = mTabbedActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_ideal);
        Bitmap splashImage = BitmapHelper.decodeBitmapFromString(dataStorageFactory.mSplashImage);
        Assert.assertEquals(idealSize, splashImage.getWidth());
        Assert.assertEquals(idealSize, splashImage.getHeight());
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledEventModalWebAppBannerCustomTab() throws Exception {
        // Sets the overridden factory to observer splash screen update.
        final TestDataStorageFactory dataStorageFactory = new TestDataStorageFactory();
        WebappDataStorage.setFactoryForTests(dataStorageFactory);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        triggerModalWebAppBanner(mCustomTabActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click_verify_appinstalled"),
                true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mCustomTabActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.Install.InstallEvent", 5 /* API_CUSTOM_TAB */));

            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            INSTALL_PATH_HISTOGRAM_NAME, /* kApiInitiatedInfobar= */ 3));
        });

        // Make sure that the splash screen icon was downloaded.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(dataStorageFactory.mSplashImage, Matchers.notNullValue());
        });

        // Test that bitmap sizes match expectations.
        int idealSize = mTabbedActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_ideal);
        Bitmap splashImage = BitmapHelper.decodeBitmapFromString(dataStorageFactory.mSplashImage);
        Assert.assertEquals(idealSize, splashImage.getWidth());
        Assert.assertEquals(idealSize, splashImage.getHeight());
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledModalNativeAppBannerBrowserTab() throws Exception {
        triggerModalNativeAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(mTestServer,
                        NATIVE_APP_MANIFEST_WITH_ID,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                NATIVE_APP_BLANK_REFERRER, true);

        // The userChoice promise should resolve (and cause the title to change). appinstalled is
        // not fired for native apps
        new TabTitleObserver(
                mTabbedActivityTestRule.getActivity().getActivityTab(), "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledModalNativeAppBannerBrowserTabWithUrl() throws Exception {
        triggerModalNativeAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(mTestServer,
                        NATIVE_APP_MANIFEST_WITH_URL,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                NATIVE_APP_REFERRER, true);

        // The userChoice promise should resolve (and cause the title to change). appinstalled is
        // not fired for native apps
        new TabTitleObserver(
                mTabbedActivityTestRule.getActivity().getActivityTab(), "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledModalNativeAppBannerCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalNativeAppBanner(mCustomTabActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(mTestServer,
                        NATIVE_APP_MANIFEST_WITH_ID,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                NATIVE_APP_BLANK_REFERRER, true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mCustomTabActivityTestRule.getActivity().getActivityTab(),
                "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedModalWebAppBannerResolvesUserChoice() throws Exception {
        triggerModalWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click"),
                false);

        // Explicitly dismiss the banner.
        final ChromeActivity activity = mTabbedActivityTestRule.getActivity();
        clickButton(activity, ButtonType.NEGATIVE);

        // Ensure userChoice is resolved.
        new TabTitleObserver(activity.getActivityTab(), "Got userChoice: dismissed")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @DisabledTest(message = "crbug.com/1144199")
    public void testBlockedModalNativeAppBannerResolveUserChoice() throws Exception {
        triggerModalNativeAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                NATIVE_APP_BLANK_REFERRER, false);

        // Explicitly dismiss the banner.
        final ChromeActivity activity = mTabbedActivityTestRule.getActivity();
        clickButton(activity, ButtonType.NEGATIVE);

        // Ensure userChoice is resolved.
        new TabTitleObserver(activity.getActivityTab(), "Got userChoice: dismissed")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalNativeAppBannerCanBeTriggeredMultipleTimesBrowserTab() throws Exception {
        triggerModalBannerMultipleTimes(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                true);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalNativeAppBannerCanBeTriggeredMultipleTimesCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalBannerMultipleTimes(mCustomTabActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                true);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerCanBeTriggeredMultipleTimesBrowserTab() throws Exception {
        triggerModalBannerMultipleTimes(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click"),
                false);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerCanBeTriggeredMultipleTimesCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalBannerMultipleTimes(mCustomTabActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click"),
                false);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR)
    public void testBlockedAmbientBadgeDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that is a PWA. The ambient badge should show.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);

        InfoBarContainer container = mTabbedActivityTestRule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.addAnimationListener(listener);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgeInfoBarAppears(mTabbedActivityTestRule);

        // Explicitly dismiss the ambient badge.
        CriteriaHelper.pollUiThread(() -> listener.mDoneAnimating);

        ArrayList<InfoBar> infobars = container.getInfoBarsForTesting();
        View close = infobars.get(0).getView().findViewById(R.id.infobar_close_button);
        TouchCommon.singleClickView(close);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Waiting two months shouldn't be long enough.
        AppBannerManager.setTimeDeltaForTesting(61);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        AppBannerManager.setTimeDeltaForTesting(62);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Waiting three months should allow the ambient badge to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgeInfoBarAppears(mTabbedActivityTestRule);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR)
    @DisabledTest(message = "Test is flaky, see crbug.com/1054196")
    public void testAmbientBadgeDoesNotAppearWhenEventCanceled() throws Exception {
        String webBannerUrl = WebappTestPage.getServiceWorkerUrlWithAction(
                mTestServer, "stash_event_and_prevent_default");
        resetEngagementForUrl(webBannerUrl, 10);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);

        // As the page called preventDefault on the beforeinstallprompt event, we do not expect to
        // see an ambient badge.
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Even after waiting for three months, there should not be no ambient badge.
        AppBannerManager.setTimeDeltaForTesting(91);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // When the page is ready and calls prompt() on the beforeinstallprompt event, only then we
        // expect to see the modal banner.
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerTriggeredWithUnsupportedNativeApp() throws Exception {
        // The web app banner should show if preferred_related_applications is true but there is no
        // supported application platform specified in the related applications list.
        triggerModalWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithManifestAndAction(mTestServer,
                        WEB_APP_MANIFEST_WITH_UNSUPPORTED_PLATFORM, "call_stashed_prompt_on_click"),
                false);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.PWA_INSTALL_USE_BOTTOMSHEET)
    public void testBottomSheet() throws Exception {
        triggerBottomSheet(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithManifest(
                        mTestServer, WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL),
                /*click=*/false);

        View toolbar = mBottomSheetController.getCurrentSheetContent().getToolbarView();
        View content = mBottomSheetController.getCurrentSheetContent().getContentView();

        // Expand the bottom sheet via drag handle.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ImageView dragHandle = toolbar.findViewById(R.id.drag_handlebar);
            TouchCommon.singleClickView(dragHandle);
        });

        waitUntilBottomSheetStatus(mTabbedActivityTestRule, BottomSheetController.SheetState.FULL);

        TextView appName =
                toolbar.findViewById(PwaInstallBottomSheetView.getAppNameViewIdForTesting());
        TextView appOrigin =
                toolbar.findViewById(PwaInstallBottomSheetView.getAppOriginViewIdForTesting());
        TextView description =
                content.findViewById(PwaInstallBottomSheetView.getDescViewIdForTesting());

        Assert.assertEquals("PWA Bottom Sheet", appName.getText());
        Assert.assertTrue(appOrigin.getText().toString().startsWith("http://127.0.0.1:"));
        Assert.assertEquals("Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                        + "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua",
                description.getText());

        // Collapse the bottom sheet.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ImageView dragHandle = toolbar.findViewById(R.id.drag_handlebar);
            TouchCommon.singleClickView(dragHandle);
        });

        waitUntilBottomSheetStatus(mTabbedActivityTestRule, BottomSheetController.SheetState.PEEK);

        // Dismiss the bottom sheet.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheetController.hideContent(
                    mBottomSheetController.getCurrentSheetContent(), false);
        });

        waitUntilBottomSheetStatus(
                mTabbedActivityTestRule, BottomSheetController.SheetState.HIDDEN);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.PWA_INSTALL_USE_BOTTOMSHEET)
    public void testAppInstalledEventBottomSheet() throws Exception {
        triggerBottomSheet(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithManifestAndAction(mTestServer,
                        WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                /*click=*/true);

        View toolbar = mBottomSheetController.getCurrentSheetContent().getToolbarView();

        // Install app from the bottom sheet.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ButtonCompat buttonInstall = toolbar.findViewById(
                    PwaInstallBottomSheetView.getButtonInstallViewIdForTesting());
            TouchCommon.singleClickView(buttonInstall);
        });

        waitUntilBottomSheetStatus(
                mTabbedActivityTestRule, BottomSheetController.SheetState.HIDDEN);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mTabbedActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.Install.InstallEvent", 4 /* API_BROWSER_TAB */));

            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            INSTALL_PATH_HISTOGRAM_NAME, /* kApiInitiateBottomSheet= */ 6));
        });
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.PWA_INSTALL_USE_BOTTOMSHEET)
    public void testDismissBottomSheetResolvesUserChoice() throws Exception {
        triggerBottomSheet(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithManifestAndAction(mTestServer,
                        WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL, "call_stashed_prompt_on_click"),
                /*click=*/true);

        // Dismiss the bottom sheet.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheetController.hideContent(
                    mBottomSheetController.getCurrentSheetContent(), false);
        });

        waitUntilBottomSheetStatus(
                mTabbedActivityTestRule, BottomSheetController.SheetState.HIDDEN);

        // Ensure userChoice is resolved.
        new TabTitleObserver(
                mTabbedActivityTestRule.getActivity().getActivityTab(), "Got userChoice: dismissed")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add({"enable-features=" + FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE,
            "disable-features=" + ChromeFeatureList.ADD_TO_HOMESCREEN_IPH})
    public void
    testInProductHelp() throws Exception {
        // Visit a site that is a PWA. The ambient badge should show.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);

        InfoBarContainer container = mTabbedActivityTestRule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.addAnimationListener(listener);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgeInfoBarAppears(mTabbedActivityTestRule);

        waitForHelpBubble(withText(R.string.iph_pwa_install_available_text)).perform(click());
        assertThat(mTracker.wasDismissed(), is(true));

        int callCount = mOnEventCallback.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuCoordinator coordinator = mTabbedActivityTestRule.getAppMenuCoordinator();
            AppMenuTestSupport.showAppMenu(coordinator, null, false);
            AppMenuTestSupport.callOnItemClick(coordinator, R.id.add_to_homescreen_id);
        });
        mOnEventCallback.waitForCallback(callCount, 1);

        assertThat(mTracker.getLastEvent(), is(EventConstants.PWA_INSTALL_MENU_SELECTED));

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    private ViewInteraction waitForHelpBubble(Matcher<View> matcher) {
        View mainDecorView = mTabbedActivityTestRule.getActivity().getWindow().getDecorView();
        return onView(isRoot())
                .inRoot(RootMatchers.withDecorView(not(is(mainDecorView))))
                .check(waitForView(matcher));
    }
}
