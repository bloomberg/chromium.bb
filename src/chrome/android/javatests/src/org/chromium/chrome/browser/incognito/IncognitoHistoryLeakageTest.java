// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.TestBrowsingHistoryObserver;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.ActivityType;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.TestParams;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Testing browsing and navigation history leaks. Browsing history leaks are checked from incognito
 * to regular mode. Navigation history leaks are checked across all tabbed and CCT activity types.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoHistoryLeakageTest {
    private EmbeddedTestServer mTestServer;
    private String mTestPage1;
    private String mTestPage2;

    private static final String TEST_PAGE_1 = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mChromeActivityTestRule =
            new ChromeActivityTestRule<>(ChromeTabbedActivity.class);

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestPage1 = mTestServer.getURL(TEST_PAGE_1);
        mTestPage2 = mTestServer.getURL(TEST_PAGE_2);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> IncognitoDataTestUtils.closeTabs(mChromeActivityTestRule));
        mTestServer.stopAndDestroyServer();
    }

    // TODO(crbug.com/1060940) : Passing a boolean is not sufficient because it can't differentiate
    // between the history returned for incognito tab and CCT when they have isolated profiles.
    private static List<HistoryItem> getBrowsingHistory(boolean incognito) throws TimeoutException {
        final TestBrowsingHistoryObserver historyObserver = new TestBrowsingHistoryObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingHistoryBridge historyService = new BrowsingHistoryBridge(incognito);
            historyService.setObserver(historyObserver);
            String historyQueryFilter = "";
            historyService.queryHistory(historyQueryFilter);
        });
        historyObserver.getQueryCallback().waitForCallback(0);
        return historyObserver.getHistoryQueryResults();
    }

    public static class AllTypesToAllTypes implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            List<ParameterSet> result = new ArrayList<>();
            result.addAll(new TestParams.IncognitoToIncognito().getParameters());
            result.addAll(new TestParams.IncognitoToRegular().getParameters());
            result.addAll(new TestParams.RegularToIncognito().getParameters());
            return result;
        }
    }

    /**
     * We test history leaks from Incognito Tab/CCT to Regular Tab/CCT and not the other way
     * round because incognito session don't have a history service of their own and rely on the
     * history service of their original regular profile. So, if we open a regular profile
     * first and visit a url and test for the url visibility in the history service provided by
     * incognito session it would always be visible as they both share the same history service.
     */
    @Test
    @LargeTest
    @UseMethodParameter(TestParams.IncognitoToRegular.class)
    public void testBrowsingHistoryDoNotLeakFromIncognitoToRegular(
            String incognitoActivityType, String regularActivityType) throws TimeoutException {
        ActivityType incognitoActivity = ActivityType.valueOf(incognitoActivityType);
        ActivityType regularActivity = ActivityType.valueOf(regularActivityType);

        // We visit the mTestPage1 from incognito
        Tab tab1 = incognitoActivity.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mTestPage1);

        List<HistoryItem> historyEntriesOfIncognitoMode = getBrowsingHistory(true);
        assertTrue(historyEntriesOfIncognitoMode.isEmpty());

        // History from regular should also be empty as well, as currently, incognito and regular
        // both share the same history service.
        assertTrue(getBrowsingHistory(false).isEmpty());

        // We visit mTestPage2 from regular
        Tab tab2 = regularActivity.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mTestPage2);

        List<HistoryItem> regularBrowsingHistory = getBrowsingHistory(false);
        assertEquals(1, regularBrowsingHistory.size());
        assertEquals(mTestPage2, regularBrowsingHistory.get(0).getUrl());

        // Since the history service is shared with incognito, getting browsing history for
        // incognito should return that of regular.
        List<HistoryItem> incognitoBrowsingHistory = getBrowsingHistory(true);
        assertEquals(1, incognitoBrowsingHistory.size());
        assertEquals(mTestPage2, incognitoBrowsingHistory.get(0).getUrl());
    }

    @Test
    @LargeTest
    @UseMethodParameter(AllTypesToAllTypes.class)
    public void testTabNavigationHistoryDoNotLeakBetweenActivities(
            String activityType1, String activityType2) throws TimeoutException {
        ActivityType activity1 = ActivityType.valueOf(activityType1);
        ActivityType activity2 = ActivityType.valueOf(activityType2);

        Tab tab1 = activity1.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mTestPage1);
        Tab tab2 = activity2.launchUrl(
                mChromeActivityTestRule, mCustomTabActivityTestRule, mTestPage2);

        CriteriaHelper.pollUiThread(() -> { assertNotNull(tab1.getWebContents()); });
        NavigationHistory navigationHistory1 =
                tab1.getWebContents().getNavigationController().getNavigationHistory();

        CriteriaHelper.pollUiThread(() -> { assertNotNull(tab2.getWebContents()); });
        NavigationHistory navigationHistory2 =
                tab2.getWebContents().getNavigationController().getNavigationHistory();

        assertEquals(1, navigationHistory1.getEntryCount());
        assertEquals(1, navigationHistory2.getEntryCount());

        NavigationEntry entry1 = navigationHistory1.getEntryAtIndex(0);
        NavigationEntry entry2 = navigationHistory2.getEntryAtIndex(0);

        assertEquals(entry1.getOriginalUrl(), mTestPage1);
        assertEquals(entry2.getOriginalUrl(), mTestPage2);
    }
}