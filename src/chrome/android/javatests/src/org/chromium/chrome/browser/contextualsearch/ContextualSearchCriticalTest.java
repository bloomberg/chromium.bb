// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests the Contextual Search Manager using instrumentation tests.
 */
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED})
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchCriticalTest extends ContextualSearchInstrumentationBase {
    /**
     * Fakes navigation of the Content View to the URL that was previously requested.
     * @param isFailure whether the request resulted in a failure.
     */
    private void fakeContentViewDidNavigate(boolean isFailure) {
        String url = mFakeServer.getLoadedUrl();
        mManager.getOverlayContentDelegate().onMainFrameNavigation(url, false, isFailure, false);
    }

    //============================================================================================
    // Test Cases
    //============================================================================================

    /**
     * Tests that only a single low-priority request is issued for a trigger/open sequence.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    @FlakyTest(message = "High priority test.  See https://crbug.com/1058297")
    public void testResolveCausesOneLowPriorityRequest(@EnabledFeature int enabledFeature)
            throws Exception {
        mFakeServer.reset();
        simulateResolveSearch("states");

        // We should not make a second-request until we get a good response from the first-request.
        assertLoadedNoUrl();
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the second request succeeds, we should not issue a new request.
        fakeContentViewDidNavigate(false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the bar opens, we should not make any additional request.
        tapPeekingBarToExpandAndAssert();
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests that a failover for a prefetch request is issued after the panel is opened.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    @DisabledTest(
            message = "TODO:donnd fix and reeenable once expanding resolve works for base tests.")
    public void
    testPrefetchFailoverRequestMadeAfterOpen(@EnabledFeature int enabledFeature) throws Exception {
        mFakeServer.reset();
        triggerResolve("states");

        // We should not make a SERP request until we get a good response from the resolve request.
        assertLoadedNoUrl();
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the second request fails, we should not automatically issue a new request.
        fakeContentViewDidNavigate(true);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Once the bar opens, we make a new request at normal priority.
        tapPeekingBarToExpandAndAssert();
        assertLoadedNormalPriorityUrl();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that a live request that fails (for an invalid URL) does a failover to a
     * normal priority request once the user triggers the failover by opening the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    @DisabledTest(message = "https://crbug.com/1075895")
    public void testLivePrefetchFailoverRequestMadeAfterOpen(@EnabledFeature int enabledFeature)
            throws Exception {
        // Test fails with out-of-process network service. crbug.com/1071721
        if (!ChromeFeatureList.isEnabled("NetworkServiceInProcess")) return;

        mFakeServer.reset();
        mFakeServer.setLowPriorityPathInvalid();
        mFakeServer.setActuallyLoadALiveSerp();
        simulateResolveSearch("search");
        assertLoadedLowPriorityInvalidUrl();
        Assert.assertTrue(mFakeServer.didAttemptLoadInvalidUrl());

        // we should not automatically issue a new request.
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Fake a navigation error if offline.
        // When connected to the Internet this error may already have happened due to actually
        // trying to load the invalid URL.  But on test bots that are not online we need to
        // fake that a navigation happened with an error. See crbug.com/682953 for details.
        if (!mManager.isOnline()) {
            boolean isFailure = true;
            fakeContentViewDidNavigate(isFailure);
        }

        // Once the bar opens, we make a new request at normal priority.
        tapPeekingBarToExpandAndAssert();
        waitForNormalPriorityUrlLoaded();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests a simple triggering gesture with disable-preload set.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    // Previously flaky and disabled 4/2021.  https://crbug.com/1192285
    @DisabledTest(
            message = "TODO:donnd fix and reeenable once expanding resolve works for base tests.")
    public void
    testResolveDisablePreload(@EnabledFeature int enabledFeature) throws Exception {
        simulateSlowResolveSearch("intelligence");

        assertSearchTermRequested();
        boolean doPreventPreload = true;
        fakeResponse(
                false, 200, "Intelligence", "display-text", "alternate-term", doPreventPreload);
        assertLoadedNoUrl();
        waitForPanelToPeek();
        assertLoadedNoUrl();
    }

    /**
     * Tests that an error from the Search Term Resolution request causes a fallback to a
     * search request for the literal selection.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/765403")
    @DisabledTest(
            message = "TODO:donnd fix and reeenable once expanding resolve works for base tests.")
    public void
    testSearchTermResolutionError(@EnabledFeature int enabledFeature) throws Exception {
        simulateSlowResolveSearch("states");
        assertSearchTermRequested();
        fakeResponse(false, 403, "", "", "", false);
        assertLoadedNoUrl();
        tapPeekingBarToExpandAndAssert();
        assertLoadedNormalPriorityUrl();
    }

    //============================================================================================
    // Content Tests
    //============================================================================================

    /**
     * Tests that resolve followed by expand makes Content visible.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    @DisabledTest(
            message = "TODO:donnd fix and reeenable once expanding resolve works for base tests.")
    public void
    testResolveContentVisibility(@EnabledFeature int enabledFeature) throws Exception {
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch();
        assertWebContentsCreatedButNeverMadeVisible();

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsVisible();

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
    }

    /**
     * Tests that a non-resolving trigger followed by panel-expand creates Content and makes it
     * visible.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    @DisabledTest(message = "http://crbug.com/1296677")
    public void testNonResolveContentVisibility(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a non-resolve search and make sure no Content is created.
        simulateNonResolveSearch("search");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
    }

    /**
     * Tests swiping panel up and down after a tap search will only load the Content once.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    // Previously flaky. See https://crbug.com/1032955
    @DisabledTest(message = "https://crbug.com/1291558")
    public void testResolveMultipleSwipeOnlyLoadsContentOnce(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should not change the visibility or load content again.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests swiping panel up and down after a non-resolving search will only load the Content
     * once.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    // Previously flaky https://crbug.com/1032760 on sdk<P.
    @DisabledTest(message = "https://crbug.com/1291558")
    public void testNonResolveMultipleSwipeOnlyLoadsContentOnce(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a non-resolve search and make sure no Content is created.
        simulateNonResolveSearch("search");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Expanding the Panel should load the URL and make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should not change the visibility or load content again.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained tap searches create new Content.
     * Chained Tap searches allow immediate triggering of a tap when quite close to a previous tap
     * selection since the user may have just missed the intended target.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    @DisabledTest(message = "http://crbug.com/1296677")
    public void testChainedSearchCreatesNewContent(@EnabledFeature int enabledFeature)
            throws Exception {
        // This test depends on preloading the content - which is loaded and not made visible.
        // We only preload when the user has decided to accept the privacy opt-in.
        mPolicy.overrideDecidedStateForTesting(true);

        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        WebContents wc1 = getPanelWebContents();

        waitToPreventDoubleTapRecognition();

        // Simulate a new resolving search and make sure new Content is created.
        simulateResolveSearch("term");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        WebContents wc2 = getPanelWebContents();
        Assert.assertNotSame(wc1, wc2);

        waitToPreventDoubleTapRecognition();

        // Simulate a new resolving search and make sure new Content is created.
        simulateResolveSearch("resolution");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
        WebContents wc3 = getPanelWebContents();
        Assert.assertNotSame(wc2, wc3);

        // Closing the Panel should destroy the Content.
        closePanel();
        assertNoWebContents();
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained searches load correctly.
     */
    @Test
    @DisabledTest(message = "crbug.com/551711")
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    public void testChainedSearchLoadsCorrectSearchTerm(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        WebContents wc1 = getPanelWebContents();

        // Expanding the Panel should make the Content visible.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        waitToPreventDoubleTapRecognition();

        // Now simulate a non-resolve search, leaving the Panel peeking.
        simulateNonResolveSearch("resolution");

        // Expanding the Panel should load and display the new search.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        WebContents wc2 = getPanelWebContents();
        Assert.assertNotSame(wc1, wc2);

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained searches make Content visible when opening the Panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled 4/2021.  https://crbug.com/1192285
    @DisabledTest(
            message = "TODO:donnd fix and reeenable once expanding resolve works for base tests.")
    public void
    testChainedSearchContentVisibility() throws Exception {
        // Chained searches are tap-triggered very close to existing tap-triggered searches.
        FeatureList.setTestFeatures(ENABLE_NONE);

        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch();
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        WebContents wc1 = getPanelWebContents();

        waitToPreventDoubleTapRecognition();

        // Now simulate a non-resolve search, leaving the Panel peeking.
        simulateNonResolveSearch("resolution");
        assertNeverCalledWebContentsOnShow();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should load and display the new search.
        tapPeekingBarToExpandAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        WebContents wc2 = getPanelWebContents();
        Assert.assertNotSame(wc1, wc2);
    }

    //============================================================================================
    // History Removal Tests.  These are important for privacy, and are not easy to test manually.
    //============================================================================================

    /**
     * Tests that a tap followed by closing the Panel removes the loaded URL from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    @DisabledTest(message = "http://crbug.com/1296677")
    public void testTapCloseRemovedFromHistory(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a resolving search and make sure a URL was loaded.
        simulateResolveSearch("search");
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        String url = mFakeServer.getLoadedUrl();

        // Close the Panel without seeing the Content.
        closePanel();

        // Now check that the URL has been removed from history.
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url));
    }

    /**
     * Tests that a tap followed by opening the Panel does not remove the loaded URL from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.O, message = "crbug.com/1184410")
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    @DisabledTest(
            message = "TODO:donnd fix and reeenable once expanding resolve works for base tests.")
    public void
    testTapExpandNotRemovedFromHistory(@EnabledFeature int enabledFeature) throws Exception {
        // Simulate a resolving search and make sure a URL was loaded.
        simulateResolveSearch();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        String url = mFakeServer.getLoadedUrl();

        // Expand Panel so that the Content becomes visible.
        tapPeekingBarToExpandAndAssert();

        // Close the Panel.
        tapBasePageToClosePanel();

        // Now check that the URL has not been removed from history, since the Content was seen.
        Assert.assertFalse(mFakeServer.hasRemovedUrl(url));
    }

    /**
     * Tests that chained searches without opening the Panel removes all loaded URLs from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.P, message = "crbug.com/1161540")
    @DisabledTest(
            message = "TODO:donnd fix and reeenable once expanding resolve works for base tests.")
    public void
    testChainedTapsRemovedFromHistory() throws Exception {
        // Make sure we use tap for the simulateResolveSearch since only tap chains.
        FeatureList.setTestFeatures(ENABLE_NONE);

        // Simulate a resolving search and make sure a URL was loaded.
        simulateResolveSearch("search");
        String url1 = mFakeServer.getLoadedUrl();
        Assert.assertNotNull(url1);

        waitToPreventDoubleTapRecognition();

        // Simulate another resolving search and make sure another URL was loaded.
        simulateResolveSearch("term");
        String url2 = mFakeServer.getLoadedUrl();
        Assert.assertNotSame(url1, url2);

        waitToPreventDoubleTapRecognition();

        // Simulate another resolving search and make sure another URL was loaded.
        simulateResolveSearch("resolution");
        String url3 = mFakeServer.getLoadedUrl();
        Assert.assertNotSame(url2, url3);

        // Close the Panel without seeing any Content.
        closePanel();

        // Now check that all three URLs have been removed from history.
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url1));
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url2));
        Assert.assertTrue(mFakeServer.hasRemovedUrl(url3));
    }
}