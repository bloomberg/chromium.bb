// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static android.support.test.espresso.Espresso.onView;

import static org.hamcrest.Matchers.instanceOf;

import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.List;

/**
 * Tests for {@link FeedNewTabPage} specifically with card rendering. Other tests can be found in
 * {@link org.chromium.chrome.browser.feed.FeedNewTabPageTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Features.EnableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
public class FeedNewTabPageCardRenderTest {
    private static final String TEST_FEED_DATA_BASE_PATH = "/chrome/test/data/android/feed/";

    @Rule
    public ChromeActivityTestRule<ChromeTabbedActivity> mActivityTestRule =
            new ChromeActivityTestRule(ChromeTabbedActivity.class);

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    @Rule
    public FeedDataInjectRule mFeedDataInjector = new FeedDataInjectRule(true);

    @Rule
    public EmbeddedTestServerRule mTestServer = new EmbeddedTestServerRule();

    private Tab mTab;
    private FeedNewTabPage mNtp;
    private ViewGroup mTileGridLayout;
    private FakeMostVisitedSites mMostVisitedSites;
    private List<SiteSuggestion> mSiteSuggestions;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSiteSuggestions = NewTabPageTestUtils.createFakeSiteSuggestions(mTestServer.getServer());
        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof FeedNewTabPage);
        mNtp = (FeedNewTabPage) mTab.getNativePage();
        mTileGridLayout = mNtp.getView().findViewById(R.id.tile_grid_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mTileGridLayout.getChildCount());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage", "RenderTest"})
    @DataFilePath(TEST_FEED_DATA_BASE_PATH + "feed_world.gcl.bin")
    public void testFeedCardRenderingScenarioWorld() throws Exception {
        renderFeedCards("world");
    }

    private void renderFeedCards(String scenarioName) throws Exception {
        // Open a new tab.
        SectionHeader firstHeader = mNtp.getMediatorForTesting().getSectionHeaderForTesting();
        RecyclerView recycleView =
                (RecyclerView) mNtp.getCoordinatorForTesting().getStream().getView();

        // Check header is expanded.
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertTrue(getPreferenceForArticleSectionHeader());

        // Trigger a refresh to get feed cards.
        mFeedDataInjector.triggerFeedRefreshOnUiThreadBlocking(mNtp.getStreamForTesting());

        // Scroll to the first feed card.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(
                        mFeedDataInjector.getFirstCardPosition()));

        RecyclerViewTestUtils.waitForStableRecyclerView(recycleView);
        mRenderTestRule.render(
                recycleView, String.format("render_feed_cards_top_%s", scenarioName));

        // Scroll to the bottom.
        RecyclerViewTestUtils.scrollToBottom(recycleView);
        RecyclerViewTestUtils.waitForStableRecyclerView(recycleView);
        mRenderTestRule.render(
                recycleView, String.format("render_feed_cards_bottom_%s", scenarioName));

        // Scroll to the first feed card again.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(
                        mFeedDataInjector.getFirstCardPosition()));
        RecyclerViewTestUtils.waitForStableRecyclerView(recycleView);
        mRenderTestRule.render(
                recycleView, String.format("render_feed_cards_top_again_%s", scenarioName));
    }

    private boolean getPreferenceForArticleSectionHeader() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE));
    }
}
