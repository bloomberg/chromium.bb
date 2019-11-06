// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import static org.chromium.chrome.browser.ChromeInactivityTracker.NTP_LAUNCH_DELAY_IN_MINS_PARAM;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.MockSafeBrowsingApiHandler;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for NoTouchActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NoTouchActivityTest {
    private static final String TEST_PATH = "/chrome/test/data/android/simple.html";
    private static final String TEST_PATH_2 = "/chrome/test/data/android/test.html";

    @Rule
    public ChromeActivityTestRule<NoTouchActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(NoTouchActivity.class);

    private EmbeddedTestServer mTestServer;
    private NoTouchActivity mActivity;

    @Before
    public void setUp() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Tests that the NoTouchActivity starts up to the NTP.
     */
    @Test
    @MediumTest
    public void testStartsUpToNewTabPage() throws Throwable {
        mActivityTestRule.startMainActivityFromLauncher();
        mActivity = mActivityTestRule.getActivity();
        Assert.assertTrue(
                mActivity.getActivityTab().getNativePage() instanceof TouchlessNewTabPage);
    }

    /**
     * Tests that the Tab persists through recreation.
     */
    @Test
    @MediumTest
    public void testRecreateWithTabHistory() throws Throwable {
        mActivityTestRule.startMainActivityFromLauncher();
        mActivity = mActivityTestRule.getActivity();

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PATH));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(mActivity.getActivityTab().getWebContents().getLastCommittedUrl(),
                    mTestServer.getURL(TEST_PATH));
        });
        mActivity = ApplicationTestUtils.recreateActivity(mActivity);
        mActivityTestRule.setActivity(mActivity);
        mActivityTestRule.waitForActivityNativeInitializationComplete();
        ChromeTabUtils.waitForTabPageLoaded(
                mActivity.getActivityTab(), mTestServer.getURL(TEST_PATH));
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivity.onBackPressed());
        CriteriaHelper.pollUiThread(
                () -> mActivity.getActivityTab().getNativePage() instanceof TouchlessNewTabPage);
    }

    /**
     * Tests that the tab does not persist after recreation, if enough time has passed.
     *
     * This test configures the time delay to be 0 minutes, which should always cause the inactivity
     * threshold to be reached upon recreation.
     */
    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"enable-features=" + ChromeInactivityTracker.FEATURE_NAME + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:" + NTP_LAUNCH_DELAY_IN_MINS_PARAM
                    + "/0"})
    public void
    testRecreateWithTabHistoryAfterInactivity() throws Throwable {
        mActivityTestRule.startMainActivityFromLauncher();
        mActivity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PATH));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(mActivity.getActivityTab().getWebContents().getLastCommittedUrl(),
                    mTestServer.getURL(TEST_PATH));
        });

        ApplicationTestUtils.fireHomeScreenIntent(mActivityTestRule.getActivity());
        ApplicationTestUtils.launchChrome(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(
                () -> mActivity.getActivityTab().getNativePage() instanceof TouchlessNewTabPage);
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"enable-features=" + ChromeInactivityTracker.FEATURE_NAME + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:" + NTP_LAUNCH_DELAY_IN_MINS_PARAM
                    + "/0"})
    public void
    testViewUrlAfterInactivity() throws Throwable {
        mActivityTestRule.startMainActivityFromLauncher();
        mActivity = mActivityTestRule.getActivity();
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PATH));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(mActivity.getActivityTab().getWebContents().getLastCommittedUrl(),
                    mTestServer.getURL(TEST_PATH));
        });

        ApplicationTestUtils.fireHomeScreenIntent(mActivityTestRule.getActivity());

        String alternateUrl = mTestServer.getURL(TEST_PATH_2);
        mActivityTestRule.startMainActivityWithURL(alternateUrl);

        CriteriaHelper.pollUiThread(
                ()
                        -> alternateUrl.equals(
                                mActivity.getActivityTab().getWebContents().getLastCommittedUrl()));
    }

    /**
     * Tests that Safe Browsing and interstitials work.
     */
    @Test
    @DisabledTest(message = "crbug.com/947232")
    public void testSafeBrowsing() throws Throwable {
        mActivityTestRule.startMainActivityFromLauncher();
        mActivity = mActivityTestRule.getActivity();

        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
        final String url = mTestServer.getURL(TEST_PATH);
        MockSafeBrowsingApiHandler.addMockResponse(url, "{\"matches\":[{\"threat_type\":\"5\"}]}");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getActivityTab().loadUrl(new LoadUrlParams(url)));
        // TODO(carlosil): For now, we check the presence of an interstitial through the title since
        // isShowingInterstitialPage does not work with committed interstitials. Once we fully
        // migrate to committed interstitials, this should be changed to a more robust check.
        CriteriaHelper.pollUiThread(
                ()
                        -> mActivity.getActivityTab().getWebContents().getTitle().equals(
                                "Security error"),
                "Failed to show Safe Browsing Interstitial page", 5000, 50);

        MockSafeBrowsingApiHandler.clearMockResponses();
    }
}
