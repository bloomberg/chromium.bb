// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.Instrumentation.ActivityResult;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.app.AlertDialog;
import android.view.View;
import android.widget.Button;

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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.engagement.SiteEngagementService;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarAnimationListener;
import org.chromium.chrome.browser.infobar.InfoBarContainerLayout.Item;
import org.chromium.chrome.browser.infobar.InstallableAmbientBadgeInfoBar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.AddToHomescreenDialog;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.WebappTestPage;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

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
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private static final String NATIVE_APP_MANIFEST_WITH_ID =
            "/chrome/test/data/banners/play_app_manifest.json";

    private static final String NATIVE_APP_MANIFEST_WITH_URL =
            "/chrome/test/data/banners/play_app_url_manifest.json";

    private static final String NATIVE_ICON_PATH = "/chrome/test/data/banners/launcher-icon-4x.png";

    private static final String NATIVE_APP_TITLE = "Mock app title";

    private static final String NATIVE_APP_INSTALL_TEXT = "Install this";

    private static final String NATIVE_APP_REFERRER = "chrome_inline&playinline=chrome_inline";

    private static final String NATIVE_APP_BLANK_REFERRER = "playinline=chrome_inline";

    private static final String INSTALL_ACTION = "INSTALL_ACTION";

    private class MockAppDetailsDelegate extends AppDetailsDelegate {
        private Observer mObserver;
        private AppData mAppData;
        private int mNumRetrieved;
        private Intent mInstallIntent;
        private String mReferrer;

        @Override
        protected void getAppDetailsAsynchronously(
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
        public void notifyAllAnimationsFinished(Item frontInfoBar) {}
    }

    private MockAppDetailsDelegate mDetailsDelegate;
    @Mock
    private PackageManager mPackageManager;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        AppBannerManager.setIsSupported(true);
        ShortcutHelper.setDelegateForTests(new ShortcutHelper.Delegate() {
            @Override
            public void addShortcutToHomescreen(
                    String title, Bitmap icon, boolean iconAdaptive, Intent shortcutIntent) {
                // Ignore to prevent adding homescreen shortcuts.
            }
        });

        mTabbedActivityTestRule.startMainActivityOnBlankPage();
        // Must be set after native has loaded.
        mDetailsDelegate = new MockAppDetailsDelegate();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { AppBannerManager.setAppDetailsDelegate(mDetailsDelegate); });

        AppBannerManager.setTotalEngagementForTesting(10);
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    private void resetEngagementForUrl(final String url, final double engagement) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            SiteEngagementService.getForProfile(Profile.getLastUsedProfile())
                    .resetBaseScoreForUrl(url, engagement);
        });
    }

    private AppBannerManager getAppBannerManager(Tab tab) {
        return AppBannerManager.forTab(tab);
    }

    private void waitForBannerManager(Tab tab) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !getAppBannerManager(tab).isRunningForTesting();
            }
        });
    }

    private void navigateToUrlAndWaitForBannerManager(
            ChromeActivityTestRule<? extends ChromeActivity> rule, String url) throws Exception {
        Tab tab = rule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(url);
        waitForBannerManager(tab);
    }

    private void waitUntilAppDetailsRetrieved(
            ChromeActivityTestRule<? extends ChromeActivity> rule, final int numExpected) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager = getAppBannerManager(rule.getActivity().getActivityTab());
                return mDetailsDelegate.mNumRetrieved == numExpected
                        && !manager.isRunningForTesting();
            }
        });
    }

    private void waitUntilAmbientBadgeInfoBarAppears(
            ChromeActivityTestRule<? extends ChromeActivity> rule) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR)) {
            CriteriaHelper.pollUiThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    List<InfoBar> infobars = rule.getInfoBars();
                    if (infobars.size() != 1) return false;
                    return infobars.get(0) instanceof InstallableAmbientBadgeInfoBar;
                }
            });
        }
    }

    private void waitUntilNoDialogsShowing(final Tab tab) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AddToHomescreenDialog dialog =
                        getAppBannerManager(tab).getAddToHomescreenDialogForTesting();
                return dialog == null || dialog.getAlertDialogForTesting() == null;
            }
        });
    }

    private void tapAndWaitForModalBanner(final Tab tab) throws Exception {
        TouchCommon.singleClickView(tab.getView());

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AddToHomescreenDialog dialog =
                        getAppBannerManager(tab).getAddToHomescreenDialogForTesting();
                if (dialog != null) {
                    AlertDialog alertDialog = dialog.getAlertDialogForTesting();
                    return alertDialog != null && alertDialog.isShowing();
                }
                return false;
            }
        });
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
        clickButton(tab, DialogInterface.BUTTON_POSITIVE);
    }

    private void triggerModalNativeAppBanner(ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url, String expectedReferrer, boolean installApp) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAppDetailsRetrieved(rule, 1);
        waitUntilAmbientBadgeInfoBarAppears(rule);
        Assert.assertEquals(mDetailsDelegate.mReferrer, expectedReferrer);

        final Tab tab = rule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);
        if (!installApp) return;

        // Click the button to trigger the installation.
        final ActivityMonitor activityMonitor =
                new ActivityMonitor(new IntentFilter(INSTALL_ACTION),
                        new ActivityResult(Activity.RESULT_OK, null), true);
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        instrumentation.addMonitor(activityMonitor);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Button button = getAppBannerManager(tab)
                                    .getAddToHomescreenDialogForTesting()
                                    .getAlertDialogForTesting()
                                    .getButton(DialogInterface.BUTTON_POSITIVE);
            Assert.assertEquals(NATIVE_APP_INSTALL_TEXT, button.getText());
        });

        clickButton(tab, DialogInterface.BUTTON_POSITIVE);

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
        clickButton(tab, DialogInterface.BUTTON_NEGATIVE);
        waitUntilNoDialogsShowing(tab);
        tapAndWaitForModalBanner(tab);

        clickButton(tab, DialogInterface.BUTTON_NEGATIVE);
        waitUntilNoDialogsShowing(tab);
        tapAndWaitForModalBanner(tab);
    }

    private void clickButton(final Tab tab, final int button) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            getAppBannerManager(tab)
                    .getAddToHomescreenDialogForTesting()
                    .getAlertDialogForTesting()
                    .getButton(button)
                    .performClick();
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
                            "Webapp.Install.InstallEvent", 4));
        });

        // Make sure that the splash screen icon was downloaded.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return dataStorageFactory.mSplashImage != null;
            }
        });

        // Test that bitmap sizes match expectations.
        int idealSize = mTabbedActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_ideal);
        Bitmap splashImage = ShortcutHelper.decodeBitmapFromString(dataStorageFactory.mSplashImage);
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
                            "Webapp.Install.InstallEvent", 5));
        });

        // Make sure that the splash screen icon was downloaded.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return dataStorageFactory.mSplashImage != null;
            }
        });

        // Test that bitmap sizes match expectations.
        int idealSize = mTabbedActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_ideal);
        Bitmap splashImage = ShortcutHelper.decodeBitmapFromString(dataStorageFactory.mSplashImage);
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
        final Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        clickButton(tab, DialogInterface.BUTTON_NEGATIVE);

        // Ensure userChoice is resolved.
        new TabTitleObserver(tab, "Got userChoice: dismissed").waitForTitleUpdate(3);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedModalNativeAppBannerResolveUserChoice() throws Exception {
        triggerModalNativeAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                NATIVE_APP_BLANK_REFERRER, false);

        // Explicitly dismiss the banner.
        final Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        clickButton(tab, DialogInterface.BUTTON_NEGATIVE);

        // Ensure userChoice is resolved.
        new TabTitleObserver(tab, "Got userChoice: dismissed").waitForTitleUpdate(3);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalNativeAppBannerCanBeTriggeredMultipleTimesBrowserTab() throws Exception {
        triggerModalBannerMultipleTimes(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                true);
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
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerCanBeTriggeredMultipleTimesBrowserTab() throws Exception {
        triggerModalBannerMultipleTimes(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click"),
                false);
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
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR)
    public void testBlockedAmbientBadgeDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that is a PWA. The ambient badge should show.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgeInfoBarAppears(mTabbedActivityTestRule);

        InfoBarContainer container = mTabbedActivityTestRule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.addAnimationListener(listener);

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
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR)
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
}
