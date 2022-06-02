// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForSecondChromeTabbedActivity;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForTabs;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.view.View;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableMap;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Manual;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchBarControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchImageControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchQuickActionControl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchInternalStateController.InternalState;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.findinpage.FindToolbar;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

// TODO(donnd): Create class with limited API to encapsulate the internals of simulations.

/**
 * Tests the Contextual Search Manager using instrumentation tests.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED,
        "disable-features=" + ChromeFeatureList.CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION + ","
                + ChromeFeatureList.CONTEXTUAL_SEARCH_THIN_WEB_VIEW_IMPLEMENTATION})
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchManagerTest extends ContextualSearchInstrumentationBase {
    /** Parameter provider for enabling/disabling triggering-related Features. */
    public static class FeatureParamProvider implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(EnabledFeature.NONE).name("default"),
                    new ParameterSet()
                            .value(EnabledFeature.TRANSLATIONS)
                            .name("enableTranslations"));
        }
    }

    // DOM element IDs in our test page based on what functions they trigger.
    // TODO(donnd): add more, and also the associated Search Term, or build a similar mapping.
    /**
     * The DOM node for the word "search" on the test page, which causes a plain search response
     * with the Search Term "Search" from the Fake server.
     */
    private static final String SIMPLE_SEARCH_NODE_ID = "search";

    /**
     * Feature maps that we use for parameterized tests.
     */

    /** This represents the current fully-launched configuration. */
    private static final ImmutableMap<String, Boolean> ENABLE_NONE = ImmutableMap.of();

    /**
     * This represents the Translations addition to the Longpress configuration.
     * This is likely the best launch candidate.
     */
    private static final ImmutableMap<String, Boolean> ENABLE_TRANSLATIONS =
            ImmutableMap.of(ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS, true);

    private UserActionTester mActionTester;

    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/tap_test.html";
        super.setUp();
    }

    @Override
    @After
    public void tearDown() throws Exception {
        if (mActionTester != null) mActionTester.tearDown();
    }

    //============================================================================================
    // UMA assertions
    //============================================================================================

    private void assertUserActionRecorded(String userActionFullName) throws Exception {
        Assert.assertTrue(mActionTester.getActions().contains(userActionFullName));
    }

    //============================================================================================
    // Test Cases
    //============================================================================================

    /**
     * Tests Ranker logging for a simple trigger that resolves.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisabledTest(message = "https://crbug.com/1291065")
    // TODO(donnd): remove with Ranker support.
    public void testResolvingSearchRankerLogging() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        simulateResolveSearch("intelligence");
        assertLoadedLowPriorityUrl();

        assertLoggedAllExpectedFeaturesToRanker();
        Assert.assertEquals(
                true, loggedToRanker(ContextualSearchInteractionRecorder.Feature.IS_LONG_WORD));
        // The panel must be closed for outcomes to be logged.
        // Close the panel by clicking far away in order to make sure the outcomes get logged by
        // the hideContextualSearchUi call to writeRankerLoggerOutcomesAndReset.
        clickWordNode("states-far");
        waitForPanelToClose();
        assertLoggedAllExpectedOutcomesToRanker();
    }

    /**
     * Tests swiping the overlay open, after an initial trigger that activates the peeking card.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/765403")
    public void testSwipeExpand() throws Exception {
        // TODO(donnd): enable for all features.
        FeatureList.setTestFeatures(ENABLE_NONE);

        assertNoSearchesLoaded();
        triggerResolve("intelligence");
        assertNoSearchesLoaded();

        // Fake a search term resolution response.
        fakeResponse(false, 200, "Intelligence", "United States Intelligence", "alternate-term",
                false);
        assertContainsParameters("Intelligence", "alternate-term");
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();

        waitForPanelToPeek();
        flingPanelUp();
        waitForPanelToExpand();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests swiping the overlay open, after an initial non-resolve search that activates the
     * peeking card, followed by closing the panel.
     * This test also verifies that we don't create any {@link WebContents} or load any URL
     * until the panel is opened.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testNonResolveSwipeExpand(@EnabledFeature int enabledFeature) throws Exception {
        simulateNonResolveSearch("search");
        assertNoWebContents();
        assertLoadedNoUrl();

        tapPeekingBarToExpandAndAssert();
        assertWebContentsCreated();
        assertLoadedNormalPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // tap the base page to close.
        closePanel();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertNoWebContents();
    }

    /**
     * Tests that a live request that fails (for an invalid URL) does a failover to a
     * normal priority request once the user triggers the failover by opening the panel.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisabledTest(message = "https://crbug.com/1140413")
    public void testLivePrefetchFailoverRequestMadeAfterOpen(@EnabledFeature int enabledFeature)
            throws Exception {
        // Test fails with out-of-process network service. crbug.com/1071721
        if (!ChromeFeatureList.isEnabled("NetworkServiceInProcess2")) return;

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
     * Tests that long-press selects text, and a subsequent tap will unselect text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testLongPressGestureSelects(@EnabledFeature int enabledFeature) throws Exception {
        longPressNode("intelligence");
        Assert.assertEquals("Intelligence", getSelectedText());
        waitForPanelToPeek();
        assertLoadedNoUrl(); // No load (preload) after long-press until opening panel.
        clickNode("question-mark");
        waitForPanelToCloseAndSelectionEmpty();
        Assert.assertTrue(TextUtils.isEmpty(getSelectedText()));
        assertLoadedNoUrl();
    }

    /**
     * Tests that a Resolve gesture selects the expected text.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    // Previously flaky, disabled 4/2021.  https://crbug.com/1192285, https://crbug.com/1192561
    public void testResolveGestureSelects(@EnabledFeature int enabledFeature) throws Exception {
        simulateResolveSearch("intelligence");
        Assert.assertEquals("Intelligence", getSelectedText());
        assertLoadedLowPriorityUrl();
        clickNode("question-mark");
        waitForPanelToClose();
        Assert.assertTrue(getSelectedText() == null || getSelectedText().isEmpty());
    }

    //============================================================================================
    // Various Tests
    //============================================================================================

    /*
     * Test that tapping on the open-new-tab icon before having a resolved search term does not
     * promote to a tab, and that after the resolution it does promote to a tab.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testPromotesToTab(@EnabledFeature int enabledFeature) throws Exception {
        // -------- SET UP ---------
        // Track Tab creation with this helper.
        final CallbackHelper tabCreatedHelper = new CallbackHelper();
        int tabCreatedHelperCallCount = tabCreatedHelper.getCallCount();
        TabModelSelectorObserver observer = new TabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                tabCreatedHelper.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().getTabModelSelector().addObserver(observer));
        // Track User Actions
        mActionTester = new UserActionTester();

        // -------- TEST ---------
        // Start a slow-resolve search and maximize the Panel.
        simulateSlowResolveSearch("search");
        maximizePanel();
        waitForPanelToMaximize();

        // A click should not promote since we are still waiting to Resolve.
        forceOpenTabIconClick();

        // Assert that the Panel is still maximized.
        waitForPanelToMaximize();

        // Let the Search Term Resolution finish.
        simulateSlowResolveFinished();

        // Now a click to promote to a separate tab.
        forceOpenTabIconClick();

        // The Panel should now be closed.
        waitForPanelToClose();

        // Make sure a tab was created.
        tabCreatedHelper.waitForCallback(tabCreatedHelperCallCount);

        // Make sure we captured the promotion in UMA.
        assertUserActionRecorded("ContextualSearch.TabPromotion");

        // -------- CLEAN UP ---------
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivityTestRule.getActivity().getTabModelSelector().removeObserver(observer);
        });
    }

    //============================================================================================
    // Undecided/Decided users.
    //============================================================================================

    /**
     * Tests that we do not resolve or preload when the privacy Opt-in has not been accepted.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testUnacceptedPrivacy(@EnabledFeature int enabledFeature) throws Exception {
        mPolicy.overrideDecidedStateForTesting(false);

        simulateResolvableSearchAndAssertResolveAndPreload("states", false);
    }

    /**
     * Tests that we do resolve and preload when the privacy Opt-in has been accepted.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testAcceptedPrivacy(@EnabledFeature int enabledFeature) throws Exception {
        mPolicy.overrideDecidedStateForTesting(true);

        simulateResolvableSearchAndAssertResolveAndPreload("states", true);
    }

    // --------------------------------------------------------------------------------------------
    // Promo open count - watches if the promo has never been opened.
    // --------------------------------------------------------------------------------------------

    /**
     * Tests the promo open counter for users that have not opted-in to privacy.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    // Only useful for disabling Tap triggering.
    @DisabledTest(message = "crbug.com/965706")
    public void testPromoOpenCountForUndecided() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        mPolicy.overrideDecidedStateForTesting(false);

        // A simple click / resolve / prefetch sequence without open should not change the counter.
        clickToTriggerPrefetch();
        Assert.assertEquals(0, mPolicy.getPromoOpenCount());

        // An open should count.
        clickToExpandAndClosePanel();
        Assert.assertEquals(1, mPolicy.getPromoOpenCount());

        // Another open should count.
        clickToExpandAndClosePanel();
        Assert.assertEquals(2, mPolicy.getPromoOpenCount());

        // Once the user has decided, we should stop counting.
        mPolicy.overrideDecidedStateForTesting(true);
        clickToExpandAndClosePanel();
        Assert.assertEquals(2, mPolicy.getPromoOpenCount());
    }

    /**
     * Tests the promo open counter for users that have already opted-in to privacy.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    public void testPromoOpenCountForDecided() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        mPolicy.overrideDecidedStateForTesting(true);

        // An open should not count for decided users.
        clickToExpandAndClosePanel();
        Assert.assertEquals(0, mPolicy.getPromoOpenCount());
    }

    // --------------------------------------------------------------------------------------------
    // Tap count - number of taps between opens.
    // --------------------------------------------------------------------------------------------
    /**
     * Tests the counter for the number of taps between opens.
     */
    @Test
    @DisabledTest(message = "crbug.com/800334")
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapCount() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        Assert.assertEquals(0, mPolicy.getTapCount());

        // A simple Tap should change the counter.
        clickToTriggerPrefetch();
        Assert.assertEquals(1, mPolicy.getTapCount());

        // Another Tap should increase the counter.
        clickToTriggerPrefetch();
        Assert.assertEquals(2, mPolicy.getTapCount());

        // An open should reset the counter.
        clickToExpandAndClosePanel();
        Assert.assertEquals(0, mPolicy.getTapCount());
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an external
     * navigation has a user gesture.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisabledTest(message = "crbug.com/1037667 crbug.com/1316518")
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testExternalNavigationWithUserGesture(@EnabledFeature int enabledFeature) {
        final ExternalNavigationDelegateImpl delegate =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        ()
                                -> new ExternalNavigationDelegateImpl(
                                        sActivityTestRule.getActivity().getActivityTab()));
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(delegate);
        GURL url = new GURL("intent://test/#Intent;scheme=test;package=com.chrome.test;end");
        final NavigationHandle navigationHandle = new NavigationHandle(
                0 /* nativeNavigationHandleProxy*/,
                new GURL("intent://test/#Intent;scheme=test;package=com.chrome.test;end"),
                GURL.emptyGURL() /* referrerUrl */, GURL.emptyGURL() /* baseUrlForDataUrl */,
                true /* isInPrimaryMainFrame */, false /* isSameDocument*/,
                true /* isRendererInitiated */, null /* initiatorOrigin */, PageTransition.LINK,
                false /* isPost */, true /* hasUserGesture */, false /* isRedirect */,
                true /* isExternalProtocol */, 0 /* navigationId */, false /* isPageActivation */);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                sActivityTestRule.getActivity().onUserInteraction();
                Assert.assertFalse(mManager.getOverlayContentDelegate().shouldInterceptNavigation(
                        externalNavHandler, navigationHandle, url));
            }
        });
        Assert.assertEquals(1, mActivityMonitor.getHits());
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an initial
     * navigation has a user gesture but the redirected external navigation doesn't.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisabledTest(message = "crbug.com/1037667 crbug.com/1316518")
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testRedirectedExternalNavigationWithUserGesture(
            @EnabledFeature int enabledFeature) {
        final ExternalNavigationDelegateImpl delegate =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        ()
                                -> new ExternalNavigationDelegateImpl(
                                        sActivityTestRule.getActivity().getActivityTab()));
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(delegate);

        GURL initialUrl = new GURL("http://test.com");
        final NavigationHandle initialNavigationHandle = new NavigationHandle(
                0 /* nativeNavigationHandleProxy*/, initialUrl, GURL.emptyGURL() /* referrerUrl */,
                GURL.emptyGURL() /* baseUrlForDataUrl */, true /* isInPrimaryMainFrame */,
                false /* isSameDocument*/, true /* isRendererInitiated */,
                null /* initiatorOrigin */, PageTransition.LINK, false /* isPost */,
                true /* hasUserGesture */, false /* isRedirect */, false /* isExternalProtocol */,
                0 /* navigationId */, false /* isPageActivation */);

        GURL redirectUrl =
                new GURL("intent://test/#Intent;scheme=test;package=com.chrome.test;end");
        final NavigationHandle redirectedNavigationHandle = new NavigationHandle(
                0 /* nativeNavigationHandleProxy*/, redirectUrl, GURL.emptyGURL() /* referrerUrl */,
                GURL.emptyGURL() /* baseUrlForDataUrl */, true /* isInPrimaryMainFrame */,
                false /* isSameDocument*/, true /* isRendererInitiated */,
                null /* initiatorOrigin */, PageTransition.LINK, false /* isPost */,
                false /* hasUserGesture */, true /* isRedirect */, true /* isExternalProtocol */,
                0 /* navigationId */, false /* isPageActivation */);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                sActivityTestRule.getActivity().onUserInteraction();
                OverlayContentDelegate delegate = mManager.getOverlayContentDelegate();
                Assert.assertTrue(delegate.shouldInterceptNavigation(
                        externalNavHandler, initialNavigationHandle, initialUrl));
                Assert.assertFalse(delegate.shouldInterceptNavigation(
                        externalNavHandler, redirectedNavigationHandle, redirectUrl));
            }
        });
        Assert.assertEquals(1, mActivityMonitor.getHits());
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an external
     * navigation doesn't have a user gesture.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testExternalNavigationWithoutUserGesture(@EnabledFeature int enabledFeature) {
        final ExternalNavigationDelegateImpl delegate =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        ()
                                -> new ExternalNavigationDelegateImpl(
                                        sActivityTestRule.getActivity().getActivityTab()));
        final ExternalNavigationHandler externalNavHandler =
                new ExternalNavigationHandler(delegate);
        GURL url = new GURL("intent://test/#Intent;scheme=test;package=com.chrome.test;end");
        final NavigationHandle navigationHandle = new NavigationHandle(
                0 /* nativeNavigationHandleProxy*/, url, GURL.emptyGURL() /* referrerUrl */,
                GURL.emptyGURL() /* baseUrlForDataUrl */, true /* isInPrimaryMainFrame */,
                false /* isSameDocument*/, true /* isRendererInitiated */,
                null /* initiatorOrigin */, PageTransition.LINK, false /* isPost */,
                false /* hasUserGesture */, false /* isRedirect */, true /* isExternalProtocol */,
                0 /* navigationId */, false /* isPageActivation */);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                sActivityTestRule.getActivity().onUserInteraction();
                Assert.assertFalse(mManager.getOverlayContentDelegate().shouldInterceptNavigation(
                        externalNavHandler, navigationHandle, url));
            }
        });
        Assert.assertEquals(0, mActivityMonitor.getHits());
    }

    //============================================================================================
    // Translate Tests
    //============================================================================================

    /**
     * Tests that a simple Tap without language determination does not trigger translation.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @FlakyTest(message = "Disabled 4/2021.  https://crbug.com/1315416")
    public void testTapWithoutLanguage(@EnabledFeature int enabledFeature) throws Exception {
        // Resolving an English word should NOT trigger translation.
        simulateResolveSearch("search");

        // Make sure we did not try to trigger translate.
        Assert.assertFalse(mManager.getRequest().isTranslationForced());
    }

    /**
     * Tests that a non-resolve search does trigger translation.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisabledTest(message = "http://crbug.com/1296677")
    public void testNonResolveTranslates(@EnabledFeature int enabledFeature) throws Exception {
        // A non-resolving gesture on any word should trigger a forced translation.
        simulateNonResolveSearch("search");
        // Make sure we did try to trigger translate.
        Assert.assertTrue(mManager.getRequest().isTranslationForced());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testSerpTranslationDisabledWhenPartialTranslationEnabled(
            @EnabledFeature int enabledFeature) throws Exception {
        // Resolving a German word should trigger translation.
        simulateResolveSearch("german");
        // Simulate a JavaScript translate message from the SERP to the manager
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.onSetCaption("caption", true));

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS)) {
            Assert.assertFalse(
                    "The SERP Translation caption should not show when Partial Translations "
                            + "is enabled!",
                    mPanel.getSearchBarControl().getCaptionVisible());
        } else {
            Assert.assertTrue(
                    "The SERP Translation caption should show without Partial Translations "
                            + "enabled!",
                    mPanel.getSearchBarControl().getCaptionVisible());
        }
    }

    /**
     * Tests the Translate Caption on a resolve gesture.
     * This test is disabled because it relies on the network and a live search result,
     * which would be flaky for bots.
     * TODO(donnd) Load a fake SERP into the panel to trigger SERP-translation and similar
     * features.
     */
    @Manual(message = "Useful for manual testing when a network is connected.")
    @Test
    @LargeTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTranslateCaption(@EnabledFeature int enabledFeature) throws Exception {
        // Resolving a German word should trigger translation.
        simulateResolveSearch("german");

        // Make sure we tried to trigger translate.
        Assert.assertTrue("Translation was not forced with the current request URL: "
                        + mManager.getRequest().getSearchUrl(),
                mManager.getRequest().isTranslationForced());

        // Wait for the translate caption to be shown in the Bar.
        int waitFactor = 5; // We need to wait an extra long time for the panel content to render.
        CriteriaHelper.pollUiThread(() -> {
            ContextualSearchBarControl barControl = mPanel.getSearchBarControl();
            Criteria.checkThat(barControl, Matchers.notNullValue());
            Criteria.checkThat(barControl.getCaptionVisible(), Matchers.is(true));
            Criteria.checkThat(barControl.getCaptionText(), Matchers.notNullValue());
            Criteria.checkThat(
                    barControl.getCaptionText().toString(), Matchers.not(Matchers.isEmptyString()));
        }, 3000 * waitFactor, DEFAULT_POLLING_INTERVAL * waitFactor);
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTranslationsFeatureCanResolveLongpressGesture() throws Exception {
        FeatureList.setTestFeatures(ENABLE_TRANSLATIONS);

        Assert.assertTrue(mPolicy.canResolveLongpress());
    }

    //============================================================================================
    // END Translate Tests
    //============================================================================================

    /**
     * Tests that Contextual Search works in fullscreen. Specifically, tests that tapping a word
     * peeks the panel, expanding the bar results in the bar ending at the correct spot in the page
     * and tapping the base page closes the panel.
     */
    @Test
    @SmallTest
    // Previously flaky and disabled 4/2021. See https://crbug.com/1197102
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTapContentAndExpandPanelInFullscreen(@EnabledFeature int enabledFeature)
            throws Exception {
        // Toggle tab to fulllscreen.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                sActivityTestRule.getActivity().getActivityTab(), true,
                sActivityTestRule.getActivity());

        // Simulate a resolving search and assert that the panel peeks.
        simulateResolveSearch("search");

        // Expand the panel and assert that it ends up in the right place.
        tapPeekingBarToExpandAndAssert();
        final ContextualSearchPanel panel =
                (ContextualSearchPanel) mManager.getContextualSearchPanel();
        Assert.assertEquals(
                panel.getHeight(), panel.getPanelHeightFromState(PanelState.EXPANDED), 0);

        // Tap the base page and assert that the panel is closed.
        tapBasePageToClosePanel();
    }

    /**
     * Tests that the Contextual Search panel is dismissed when entering or exiting fullscreen.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisableIf.Device(type = {UiDisableIf.PHONE}) // Flaking on phones crbug.com/765796
    public void testPanelDismissedOnToggleFullscreen(@EnabledFeature int enabledFeature)
            throws Exception {
        // Simulate a resolving search and assert that the panel peeks.
        simulateResolveSearch("search");

        // Toggle tab to fullscreen.
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, sActivityTestRule.getActivity());

        // Assert that the panel is closed.
        waitForPanelToClose();

        // Simulate a resolving search and assert that the panel peeks.
        simulateResolveSearch("search");

        // Toggle tab to non-fullscreen.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, false, sActivityTestRule.getActivity());

        // Assert that the panel is closed.
        waitForPanelToClose();
    }

    /**
     * Tests that ContextualSearchImageControl correctly sets either the icon sprite or thumbnail
     * as visible.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testImageControl(@EnabledFeature int enabledFeature) throws Exception {
        simulateResolveSearch("search");

        final ContextualSearchImageControl imageControl = mPanel.getImageControl();

        Assert.assertFalse(imageControl.getThumbnailVisible());
        Assert.assertTrue(TextUtils.isEmpty(imageControl.getThumbnailUrl()));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            imageControl.setThumbnailUrl("http://someimageurl.com/image.png");
            imageControl.onThumbnailFetched(true);
        });

        Assert.assertTrue(imageControl.getThumbnailVisible());
        Assert.assertEquals(imageControl.getThumbnailUrl(), "http://someimageurl.com/image.png");

        TestThreadUtils.runOnUiThreadBlocking(() -> imageControl.hideCustomImage(false));

        Assert.assertFalse(imageControl.getThumbnailVisible());
        Assert.assertTrue(TextUtils.isEmpty(imageControl.getThumbnailUrl()));
    }

    // TODO(twellington): Add an end-to-end integration test for fetching a thumbnail based on a
    //                    a URL that is included with the resolution response.

    /**
     * Tests that the quick action caption is set correctly when one is available. Also tests that
     * the caption gets changed when the panel is expanded and reset when the panel is closed.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisabledTest(message = "https://crbug.com/1291558")
    public void testQuickActionCaptionAndImage(@EnabledFeature int enabledFeature)
            throws Exception {
        CompositorAnimationHandler.setTestingMode(true);

        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPanel.onSearchTermResolved("search", null, "tel:555-555-5555",
                                QuickActionCategory.PHONE, CardTag.CT_CONTACT,
                                null /* relatedSearchesInBar */, false /* showDefaultSearchInBar */,
                                null /* relatedSearchesInContent */,
                                false /* showDefaultSearchInContent */));

        ContextualSearchBarControl barControl = mPanel.getSearchBarControl();
        ContextualSearchQuickActionControl quickActionControl = barControl.getQuickActionControl();
        ContextualSearchImageControl imageControl = mPanel.getImageControl();

        // Check that the peeking bar is showing the quick action data.
        Assert.assertTrue(quickActionControl.hasQuickAction());
        Assert.assertTrue(barControl.getCaptionVisible());
        Assert.assertEquals(sActivityTestRule.getActivity().getResources().getString(
                                    R.string.contextual_search_quick_action_caption_phone),
                barControl.getCaptionText());
        Assert.assertEquals(1.f, imageControl.getCustomImageVisibilityPercentage(), 0);

        // Expand the bar.
        TestThreadUtils.runOnUiThreadBlocking(() -> mPanel.simulateTapOnEndButton());
        waitForPanelToExpand();

        // Check that the expanded bar is showing the correct image.
        Assert.assertEquals(0.f, imageControl.getCustomImageVisibilityPercentage(), 0);

        // Go back to peeking.
        swipePanelDown();
        waitForPanelToPeek();

        // Assert that the quick action data is showing.
        Assert.assertTrue(barControl.getCaptionVisible());
        Assert.assertEquals(sActivityTestRule.getActivity().getResources().getString(
                                    R.string.contextual_search_quick_action_caption_phone),
                barControl.getCaptionText());
        // TODO(donnd): figure out why we get ~0.65 on Oreo rather than 1. https://crbug.com/818515.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            Assert.assertEquals(1.f, imageControl.getCustomImageVisibilityPercentage(), 0);
        } else {
            Assert.assertTrue(0.5f < imageControl.getCustomImageVisibilityPercentage());
        }

        CompositorAnimationHandler.setTestingMode(false);
    }

    /**
     * Tests that an intent is sent when the bar is tapped and a quick action is available.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @FlakyTest(message = "Disabled 4/2021.  https://crbug.com/1315417")
    public void testQuickActionIntent(@EnabledFeature int enabledFeature) throws Exception {
        // Add a new filter to the activity monitor that matches the intent that should be fired.
        IntentFilter quickActionFilter = new IntentFilter(Intent.ACTION_VIEW);
        quickActionFilter.addDataScheme("tel");

        // Note that we don't reuse mActivityMonitor here or we would leak the one already added
        // (unless we removed it here first). When ActivityMonitors leak, Instrumentation silently
        // ignores matching ones added after and tests will fail.
        ActivityMonitor activityMonitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(quickActionFilter,
                        new Instrumentation.ActivityResult(Activity.RESULT_OK, null), true);

        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPanel.onSearchTermResolved("search", null, "tel:555-555-5555",
                                QuickActionCategory.PHONE, CardTag.CT_CONTACT,
                                null /* relatedSearchesInBar */, false /* showDefaultSearchInBar */,
                                null /* relatedSearchesInContent */,
                                false /* showDefaultSearchInContent */));

        sActivityTestRule.getActivity().onUserInteraction();
        retryPanelBarInteractions(() -> {
            // Tap on the portion of the bar that should trigger the quick action intent to be
            // fired.
            clickPanelBar();

            // Assert that an intent was fired.
            Assert.assertEquals(1, activityMonitor.getHits());
        }, false);
        InstrumentationRegistry.getInstrumentation().removeMonitor(activityMonitor);
    }

    /**
     * Tests that the current tab is navigated to the quick action URI for
     * QuickActionCategory#WEBSITE.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.O, message = "crbug.com/1075895")
    @DisabledTest(message = "Flaky https://crbug.com/1127796")
    public void testQuickActionUrl(@EnabledFeature int enabledFeature) throws Exception {
        final String testUrl = mTestServer.getURL("/chrome/test/data/android/google.html");

        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPanel.onSearchTermResolved("search", null, testUrl,
                                QuickActionCategory.WEBSITE, CardTag.CT_URL,
                                null /* relatedSearchesInBar */, false /* showDefaultSearchInBar */,
                                null /* relatedSearchesInContent */,
                                false /* showDefaultSearchInContent */));
        retryPanelBarInteractions(() -> {
            // Tap on the portion of the bar that should trigger the quick action.
            clickPanelBar();

            // Assert that the URL was loaded.
            ChromeTabUtils.waitForTabPageLoaded(
                    sActivityTestRule.getActivity().getActivityTab(), testUrl);
        }, false);
    }

    private void runDictionaryCardTest(@CardTag int cardTag) throws Exception {
        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPanel.onSearchTermResolved("obscure · əbˈskyo͝or", null, null,
                                QuickActionCategory.NONE, cardTag, null /* relatedSearchesInBar */,
                                false /* showDefaultSearchInBar */,
                                null /* relatedSearchesInContent */,
                                false /* showDefaultSearchInContent */));

        tapPeekingBarToExpandAndAssert();
    }

    /**
     * Tests that the flow for showing dictionary definitions works, and that tapping in the
     * bar just opens the panel instead of taking some action.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisabledTest(message = "http://crbug.com/1296677")
    public void testDictionaryDefinitions(@EnabledFeature int enabledFeature) throws Exception {
        runDictionaryCardTest(CardTag.CT_DEFINITION);
    }

    /**
     * Tests that the flow for showing dictionary definitions works, and that tapping in the
     * bar just opens the panel instead of taking some action.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testContextualDictionaryDefinitions(@EnabledFeature int enabledFeature)
            throws Exception {
        runDictionaryCardTest(CardTag.CT_CONTEXTUAL_DEFINITION);
    }

    /**
     * Tests accessibility mode: Tap and Long-press don't activate CS.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testAccesibilityMode(@EnabledFeature int enabledFeature) throws Exception {
        mManager.onAccessibilityModeChanged(true);

        // Simulate a tap that resolves to show the Bar.
        clickNode("intelligence");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Simulate a Long-press.
        longPressNodeWithoutWaiting("states");
        assertNoWebContents();
        assertNoSearchesLoaded();
        mManager.onAccessibilityModeChanged(false);
    }

    /**
     * Tests when FirstRun is not completed: Tap and Long-press don't activate CS.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testFirstRunNotCompleted(@EnabledFeature int enabledFeature) throws Exception {
        // Store the original value in a temp, and mark the first run as not completed
        // for this test case.
        // Getting value from shared preference rather than FirstRunStatus#getFirstRunFlowComplete
        // to get rid of the impact from commandline switch. See https://crbug.com/1158467
        boolean originalIsFirstRunComplete = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, false);
        FirstRunStatus.setFirstRunFlowComplete(false);

        // Simulate a tap that resolves to show the Bar.
        clickNode("intelligence");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Simulate a Long-press.
        longPressNodeWithoutWaiting("states");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Restore the original shared preference value before this test case ends.
        FirstRunStatus.setFirstRunFlowComplete(originalIsFirstRunComplete);
    }

    //============================================================================================
    // Internal State Controller tests, which ensure that the internal logic flows as expected for
    // each type of triggering gesture.
    //============================================================================================

    /**
     * Tests that the Manager cycles through all the expected Internal States on Tap that Resolves.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled 4/2021.  https://crbug.com/1058297
    public void testAllInternalStatesVisitedResolvingTap() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a gesture that resolves to show the Bar.
        simulateResolveSearch("search");

        Assert.assertEquals("Some states were started but never finished",
                internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The resolving Tap gesture did not sequence through the expected states.",
                ContextualSearchInternalStateControllerWrapper.EXPECTED_TAP_RESOLVE_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The Tap gesture did not trigger a resolved search, or the resolve sequence did "
                        + "not complete.",
                InternalState.SEARCH_COMPLETED, internalStateControllerWrapper.getState());
    }

    /**
     * Tests that the Manager cycles through all the expected Internal States on Long-press that
     * Resolves.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testAllInternalStatesVisitedResolvingLongpress(@EnabledFeature int enabledFeature)
            throws Exception {
        if (!mPolicy.canResolveLongpress()) return;

        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a resolving search to show the Bar.
        longPressNode(SIMPLE_SEARCH_NODE_ID);
        fakeAResponse();

        Assert.assertEquals("Some states were started but never finished",
                internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The resolving Long-press gesture did not sequence through the expected states.",
                ContextualSearchInternalStateControllerWrapper.EXPECTED_LONGPRESS_RESOLVE_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The Long-press gesture did not trigger a resolved search, or the resolve sequence "
                        + "did not complete.",
                InternalState.SEARCH_COMPLETED, internalStateControllerWrapper.getState());
    }

    /**
     * Tests that the Manager cycles through all the expected Internal States on a Long-press.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled 4/2021.  https://crbug.com/1192285
    @DisabledTest(
            message = "TODO(donnd): reenable after unifying resolving and non-resolving longpress.")
    public void
    testAllInternalStatesVisitedNonResolveLongpress() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);

        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a Long-press to show the Bar.
        simulateNonResolveSearch("search");

        Assert.assertEquals("Some states were started but never finished",
                internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The non-resolving Long-press gesture didn't sequence through all of the expected "
                        + " states.",
                ContextualSearchInternalStateControllerWrapper.EXPECTED_LONGPRESS_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
    }

    //============================================================================================
    // Various tests
    //============================================================================================

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    // Previously flaky and disabled 4/2021.  https://crbug.com/1180304
    public void testTriggeringContextualSearchHidesFindInPageOverlay(
            @EnabledFeature int enabledFeature) throws Exception {
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), R.id.find_in_page_id);

        CriteriaHelper.pollUiThread(() -> {
            FindToolbar findToolbar =
                    (FindToolbar) sActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
            Criteria.checkThat(findToolbar, Matchers.notNullValue());
            Criteria.checkThat(findToolbar.isShown(), Matchers.is(true));
            Criteria.checkThat(findToolbar.isAnimating(), Matchers.is(false));
        });

        // Don't type anything to Find because that may cause scrolling which makes clicking in the
        // page flaky.

        View findToolbar = sActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
        Assert.assertTrue(findToolbar.isShown());

        simulateResolveSearch("search");

        waitForPanelToPeek();
        Assert.assertFalse(
                "Find Toolbar should no longer be shown once Contextual Search Panel appeared",
                findToolbar.isShown());
    }

    /**
     * Tests Tab reparenting.  When a tab moves from one activity to another the
     * ContextualSearchTabHelper should detect the change and handle gestures for it too.  This
     * happens with multiwindow modes.
     */
    @Test
    @LargeTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    @MaxAndroidSdkLevel(value = Build.VERSION_CODES.R, reason = "crbug.com/1301017")
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testTabReparenting(@EnabledFeature int enabledFeature) throws Exception {
        // Move our "tap_test" tab to another activity.
        final ChromeActivity ca = sActivityTestRule.getActivity();

        // Create a new tab so |ca| isn't destroyed.
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), ca);
        ChromeTabUtils.switchTabInCurrentTabModel(ca, 0);

        int testTabId = ca.getActivityTab().getId();
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(), ca,
                R.id.move_to_other_window_menu_id);

        // Wait for the second activity to start up and be ready for interaction.
        final ChromeTabbedActivity activity2 = waitForSecondChromeTabbedActivity();
        waitForTabs("CTA2", activity2, 1, testTabId);

        // Trigger on a word and wait for the selection to be established.
        triggerNode(activity2.getActivityTab(), "search");
        CriteriaHelper.pollUiThread(() -> {
            String selection = activity2.getContextualSearchManagerSupplier()
                                       .get()
                                       .getSelectionController()
                                       .getSelectedText();
            Criteria.checkThat(selection, Matchers.is("Search"));
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> activity2.getCurrentTabModel().closeAllTabs());
        ApplicationTestUtils.finishActivity(activity2);
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // TODO(donnd): Investigate support for logging user interactions for Long-press.
    public void testLoggedEventId() throws Exception {
        FeatureList.setTestFeatures(ENABLE_NONE);
        mFakeServer.reset();
        simulateResolveSearch("intelligence-logged-event-id");
        tapPeekingBarToExpandAndAssert();
        closePanel();
        // Now the event and outcome should be in local storage.
        simulateResolveSearch("search");
        // Check that we sent the logged event ID and outcome with the request.
        Assert.assertEquals(ContextualSearchFakeServer.LOGGED_EVENT_ID,
                mManager.getContext().getPreviousEventId());
        Assert.assertEquals(1, mManager.getContext().getPreviousUserInteractions());
        closePanel();
        // Now that we've sent them to the server, the local storage should be clear.
        simulateResolveSearch("search");
        Assert.assertEquals(0, mManager.getContext().getPreviousEventId());
        Assert.assertEquals(0, mManager.getContext().getPreviousUserInteractions());
        closePanel();
        // Make sure a duration was recorded in bucket 0 (due to 0 days duration running this test).
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Search.ContextualSearch.OutcomesDuration", 0));
    }

    // --------------------------------------------------------------------------------------------
    // Longpress-resolve Feature tests: force long-press experiment and make sure that triggers.
    // --------------------------------------------------------------------------------------------
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapIsIgnoredWithLongpressResolveEnabled() throws Exception {
        // Enabling Translations implicitly enables Longpress too.
        FeatureList.setTestFeatures(ENABLE_TRANSLATIONS);

        clickNode("states");
        Assert.assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        assertLoadedNoUrl();
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testLongpressResolveEnabled() throws Exception {
        // Enabling Translations implicitly enables Longpress too.
        FeatureList.setTestFeatures(ENABLE_TRANSLATIONS);

        longPressNode("states");
        assertLoadedNoUrl();
        assertSearchTermRequested();

        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        assertContainsParameters("states", "alternate-term");
    }

    /**
     * Monitor user action UMA recording operations.
     */
    private static class UserActionMonitor extends UserActionTester {
        // TODO(donnd): merge into UserActionTester. See https://crbug.com/1103757.
        private Set<String> mUserActionPrefixes;
        private Map<String, Integer> mUserActionCounts;

        /** @param userActionPrefixes A set of plain prefix strings for user actions to monitor. */
        UserActionMonitor(Set<String> userActionPrefixes) {
            mUserActionPrefixes = userActionPrefixes;
            mUserActionCounts = new HashMap<String, Integer>();
            for (String action : mUserActionPrefixes) {
                mUserActionCounts.put(action, 0);
            }
        }

        @Override
        public void onActionRecorded(String action) {
            for (String entry : mUserActionPrefixes) {
                if (action.startsWith(entry)) {
                    mUserActionCounts.put(entry, mUserActionCounts.get(entry) + 1);
                }
            }
        }

        /**
         * Gets the count of user actions recorded for the given prefix.
         * @param actionPrefix The plain string prefix to lookup (must match a constructed entry)
         * @return The count of user actions recorded for that prefix.
         */
        int get(String actionPrefix) {
            return mUserActionCounts.get(actionPrefix);
        }
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisabledTest(message = "https://crbug.com/1048827, https://crbug.com/1181088")
    public void testLongpressExtendingSelectionExactResolve() throws Exception {
        // Enabling Translations implicitly enables Longpress too.
        FeatureList.setTestFeatures(ENABLE_TRANSLATIONS);

        // Set up UserAction monitoring.
        Set<String> userActions = new HashSet();
        userActions.add("ContextualSearch.SelectionEstablished");
        userActions.add("ContextualSearch.ManualRefine");
        UserActionMonitor userActionMonitor = new UserActionMonitor(userActions);

        // First test regular long-press.  It should not require an exact resolve.
        longPressNode("search");
        fakeAResponse();
        assertSearchTermRequested();
        assertExactResolve(false);

        // Long press a node without release so we can simulate the user extending the selection.
        long downTime = longPressNodeWithoutUp("search");

        // Extend the selection to the nearby word.
        longPressExtendSelection("term", "resolution", downTime);
        waitForSelectActionBarVisible();
        fakeAResponse();
        assertSearchTermRequested();
        assertExactResolve(true);

        // Check UMA metrics recorded.
        Assert.assertEquals(2, userActionMonitor.get("ContextualSearch.ManualRefine"));
        Assert.assertEquals(2, userActionMonitor.get("ContextualSearch.SelectionEstablished"));
    }
}
