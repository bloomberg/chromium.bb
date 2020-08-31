// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webapk.lib.client.WebApkVersion;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tests WebApkUpdateManager. This class contains tests which cannot be done as JUnit tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP})
public class WebApkUpdateManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final String WEBAPK_ID = "webapk_id";
    private static final String WEBAPK_MANIFEST_URL =
            "/chrome/test/data/banners/manifest_one_icon.json";
    private static final String WEBAPK_MANIFEST_TOO_MANY_SHORTCUTS_URL =
            "/chrome/test/data/banners/manifest_too_many_shortcuts.json";

    // manifest_one_icon_maskable.json is the same as manifest_one_icon.json except that it has an
    // additional icon of purpose maskable and of same size.
    private static final String WEBAPK_MANIFEST_WITH_MASKABLE_ICON_URL =
            "/chrome/test/data/banners/manifest_maskable.json";

    // Data contained in {@link WEBAPK_MANIFEST_URL}.
    private static final String WEBAPK_START_URL =
            "/chrome/test/data/banners/manifest_test_page.html";
    private static final String WEBAPK_SCOPE_URL = "/chrome/test/data/banners/";
    private static final String WEBAPK_NAME = "Manifest test app";
    private static final String WEBAPK_SHORT_NAME = "Manifest test app";
    private static final String WEBAPK_ICON_URL = "/chrome/test/data/banners/image-512px.png";
    private static final String WEBAPK_ICON_MURMUR2_HASH = "7742433188808797392";
    private static final @WebDisplayMode int WEBAPK_DISPLAY_MODE = WebDisplayMode.STANDALONE;
    private static final int WEBAPK_ORIENTATION = ScreenOrientationValues.LANDSCAPE;
    private static final long WEBAPK_THEME_COLOR = 2147483648L;
    private static final long WEBAPK_BACKGROUND_COLOR = 2147483648L;

    private ChromeActivity mActivity;
    private Tab mTab;
    private EmbeddedTestServer mTestServer;

    /**
     * Subclass of {@link WebApkUpdateManager} which notifies the {@link CallbackHelper} passed to
     * the constructor when it has been determined whether an update is needed.
     */
    private static class TestWebApkUpdateManager extends WebApkUpdateManager {
        private CallbackHelper mWaiter;
        private boolean mNeedsUpdate;

        public TestWebApkUpdateManager(CallbackHelper waiter, ChromeActivity activity,
                ActivityLifecycleDispatcher lifecycleDispatcher) {
            super(activity, lifecycleDispatcher);
            mWaiter = waiter;
        }

        @Override
        public void onGotManifestData(BrowserServicesIntentDataProvider fetchedInfo,
                String primaryIconUrl, String splashIconUrl) {
            super.onGotManifestData(fetchedInfo, primaryIconUrl, splashIconUrl);
            mWaiter.notifyCalled();
        }

        @Override
        protected void storeWebApkUpdateRequestToFile(String updateRequestPath, WebappInfo info,
                String primaryIconUrl, String splashIconUrl, boolean isManifestStale,
                @WebApkUpdateReason int updateReason, Callback<Boolean> callback) {
            mNeedsUpdate = true;
        }

        public boolean needsUpdate() {
            return mNeedsUpdate;
        }
    }

    private static class CreationData {
        public String manifestUrl;
        public String startUrl;
        public String scope;
        public String name;
        public String shortName;
        public Map<String, String> iconUrlToMurmur2HashMap;
        public @WebDisplayMode int displayMode;
        public int orientation;
        public long themeColor;
        public long backgroundColor;
        public boolean isPrimaryIconMaskable;
        public List<WebApkExtras.ShortcutItem> shortcuts;
    }

    public CreationData defaultCreationData() {
        CreationData creationData = new CreationData();
        creationData.manifestUrl = mTestServer.getURL(WEBAPK_MANIFEST_URL);
        creationData.startUrl = mTestServer.getURL(WEBAPK_START_URL);
        creationData.scope = mTestServer.getURL(WEBAPK_SCOPE_URL);
        creationData.name = WEBAPK_NAME;
        creationData.shortName = WEBAPK_SHORT_NAME;

        creationData.iconUrlToMurmur2HashMap = new HashMap<String, String>();
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL(WEBAPK_ICON_URL), WEBAPK_ICON_MURMUR2_HASH);

        creationData.displayMode = WEBAPK_DISPLAY_MODE;
        creationData.orientation = WEBAPK_ORIENTATION;
        creationData.themeColor = WEBAPK_THEME_COLOR;
        creationData.backgroundColor = WEBAPK_BACKGROUND_COLOR;
        creationData.isPrimaryIconMaskable = false;
        creationData.shortcuts = new ArrayList<>();
        return creationData;
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mTab = mActivity.getActivityTab();
        mTestServer = mTestServerRule.getServer();

        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(WEBAPK_ID, callback);
        callback.waitForCallback(0);
    }

     /** Checks whether a WebAPK update is needed. */
    private boolean checkUpdateNeeded(final CreationData creationData) throws Exception {
        CallbackHelper waiter = new CallbackHelper();
        final TestWebApkUpdateManager updateManager =
                new TestWebApkUpdateManager(waiter, mActivity, mActivity.getLifecycleDispatcher());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebappDataStorage storage =
                    WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);
            BrowserServicesIntentDataProvider intentDataProvider =
                    WebApkIntentDataProviderFactory.create("", creationData.scope, null, null,
                            creationData.name, creationData.shortName, creationData.displayMode,
                            creationData.orientation, 0, creationData.themeColor,
                            creationData.backgroundColor, 0, creationData.isPrimaryIconMaskable,
                            false /* isSplashIconMaskable */, "",
                            WebApkVersion.REQUEST_UPDATE_FOR_SHELL_APK_VERSION,
                            creationData.manifestUrl, creationData.startUrl,
                            WebApkDistributor.BROWSER, creationData.iconUrlToMurmur2HashMap, null,
                            false /* forceNavigation */, false /* isSplashProvidedByWebApk */,
                            null /* shareData */, creationData.shortcuts,
                            1 /* webApkVersionCode */);
            updateManager.updateIfNeeded(storage, intentDataProvider);
        });
        waiter.waitForCallback(0);

        return updateManager.needsUpdate();
    }

    /**
     * Test that the canonicalized URLs are used in determining whether the fetched Web Manifest
     * data differs from the metadata in the WebAPK's Android Manifest. This is important because
     * the URLs in the Web Manifest have been modified by the WebAPK server prior to being stored in
     * the WebAPK Android Manifest. Chrome and the WebAPK server parse URLs differently.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testCanonicalUrlsIdenticalShouldNotUpgrade() throws Exception {
        // URL canonicalization should replace "%74" with 't'.
        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_%74est_page.html");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertFalse(checkUpdateNeeded(creationData));
    }

    /**
     * Test that an upgraded WebAPK is requested if the canonicalized "start URLs" are different.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testCanonicalUrlsDifferentShouldUpgrade() throws Exception {
        // URL canonicalization should replace "%62" with 'b'.
        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_%62est_page.html");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertTrue(checkUpdateNeeded(creationData));
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testNoUpdateForPagesWithoutWST() throws Exception {
        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_test_page.html");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_URL);
        Assert.assertFalse(checkUpdateNeeded(creationData));
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    @Features.EnableFeatures(ChromeFeatureList.WEBAPK_ADAPTIVE_ICON)
    public void testNewMaskableIconShouldUpdateWhenFeatureEnabled() throws Exception {
        testNewMaskableIconShouldUpdate();
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    @Features.DisableFeatures(ChromeFeatureList.WEBAPK_ADAPTIVE_ICON)
    public void testNewMaskableIconShouldUpdateWhenFeatureDisabled() throws Exception {
        testNewMaskableIconShouldUpdate();
    }

    private void testNewMaskableIconShouldUpdate() throws Exception {
        final String maskableManifestUrl = "/chrome/test/data/banners/manifest_maskable.json";

        CreationData creationData = new CreationData();
        creationData.manifestUrl = mTestServer.getURL(maskableManifestUrl);
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_test_page.html");
        creationData.scope = mTestServer.getURL("/chrome/test/data/banners/");
        creationData.name = "Manifest test app";
        creationData.shortName = creationData.name;

        creationData.iconUrlToMurmur2HashMap = new HashMap<String, String>();
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL("/chrome/test/data/banners/launcher-icon-4x.png"),
                "8692598279279335241");
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServer.getURL("/chrome/test/data/banners/launcher-icon-3x.png"),
                "16812314236514539104");
        creationData.displayMode = WebDisplayMode.STANDALONE;
        creationData.orientation = ScreenOrientationValues.LANDSCAPE;
        creationData.themeColor = 2147483648L;
        creationData.backgroundColor = 2147483648L;
        creationData.isPrimaryIconMaskable = false;
        creationData.shortcuts = new ArrayList<>();

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, maskableManifestUrl);

        Assert.assertEquals(
                ShortcutHelper.doesAndroidSupportMaskableIcons(), checkUpdateNeeded(creationData));
    }

    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testManifestWithExtraShortcutsDoesNotCauseUpdate() throws Exception {
        CreationData creationData = defaultCreationData();
        creationData.startUrl =
                mTestServer.getURL("/chrome/test/data/banners/manifest_test_page.html");

        creationData.manifestUrl = mTestServer.getURL(WEBAPK_MANIFEST_TOO_MANY_SHORTCUTS_URL);
        for (int i = 0; i < 4; i++) {
            creationData.shortcuts.add(new WebApkExtras.ShortcutItem("name" + String.valueOf(i),
                    "short_name", mTestServer.getURL(WEBAPK_SCOPE_URL + "launch_url"), "", "",
                    new WebappIcon()));
        }

        // The fifth shortcut should be ignored.
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServer, mTab, WEBAPK_MANIFEST_TOO_MANY_SHORTCUTS_URL);
        Assert.assertFalse(checkUpdateNeeded(creationData));
    }
}
