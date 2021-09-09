// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwWebContentsObserver;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

/**
 * Tests for the AwWebContentsObserver class.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwWebContentsObserverTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwWebContentsObserver mWebContentsObserver;

    private GURL mExampleURL;
    private GURL mExampleURLWithFragment;
    private GURL mSyncURL;
    private GURL mUnreachableWebDataUrl;

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mUnreachableWebDataUrl = new GURL(AwContentsStatics.getUnreachableWebDataUrl());
        mExampleURL = new GURL("http://www.example.com/");
        mExampleURLWithFragment = new GURL("http://www.example.com/#anchor");
        mSyncURL = new GURL("http://example.org/");
        // AwWebContentsObserver constructor must be run on the UI thread.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mWebContentsObserver =
                                   new AwWebContentsObserver(mTestContainerView.getWebContents(),
                                           mTestContainerView.getAwContents(), mContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinished() throws Throwable {
        int frameId = 0;
        boolean mainFrame = true;
        boolean subFrame = false;
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        int callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, mExampleURL, true, mainFrame);
        mWebContentsObserver.didStopLoading(mExampleURL, true);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals("onPageFinished should be called for main frame navigations.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        Assert.assertEquals("onPageFinished should be called for main frame navigations.",
                mExampleURL.getSpec(), onPageFinishedHelper.getUrl());

        // In order to check that callbacks are *not* firing, first we execute code
        // that shoudn't emit callbacks, then code that emits a callback, and check that we
        // have got only one callback, and that its URL is from the last call. Since
        // callbacks are serialized, that means we didn't have a callback for the first call.
        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, mExampleURL, true, subFrame);
        mWebContentsObserver.didFinishLoad(frameId, mSyncURL, true, mainFrame);
        mWebContentsObserver.didStopLoading(mSyncURL, true);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals("onPageFinished should only be called for the main frame.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        Assert.assertEquals("onPageFinished should only be called for the main frame.",
                mSyncURL.getSpec(), onPageFinishedHelper.getUrl());

        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, mUnreachableWebDataUrl, false, mainFrame);
        mWebContentsObserver.didFinishLoad(frameId, mSyncURL, true, mainFrame);
        mWebContentsObserver.didStopLoading(mSyncURL, true);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals("onPageFinished should not be called for the error url.", callCount + 1,
                onPageFinishedHelper.getCallCount());
        Assert.assertEquals("onPageFinished should not be called for the error url.",
                mSyncURL.getSpec(), onPageFinishedHelper.getUrl());

        String baseUrl = null;
        boolean isInMainFrame = true;
        boolean isErrorPage = false;
        boolean isSameDocument = true;
        boolean fragmentNavigation = true;
        boolean isRendererInitiated = true;
        int errorCode = 0;
        int httpStatusCode = 200;
        callCount = onPageFinishedHelper.getCallCount();
        simulateNavigation(mExampleURL, isInMainFrame, isErrorPage, !isSameDocument,
                !fragmentNavigation, !isRendererInitiated, PageTransition.TYPED);
        simulateNavigation(mExampleURLWithFragment, isInMainFrame, isErrorPage, isSameDocument,
                fragmentNavigation, isRendererInitiated, PageTransition.TYPED);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals("onPageFinished should be called for main frame fragment navigations.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        Assert.assertEquals("onPageFinished should be called for main frame fragment navigations.",
                mExampleURLWithFragment.getSpec(), onPageFinishedHelper.getUrl());

        callCount = onPageFinishedHelper.getCallCount();
        simulateNavigation(mExampleURL, isInMainFrame, isErrorPage, !isSameDocument,
                !fragmentNavigation, !isRendererInitiated, PageTransition.TYPED);
        mWebContentsObserver.didFinishLoad(frameId, mSyncURL, true, mainFrame);
        mWebContentsObserver.didStopLoading(mSyncURL, true);
        onPageFinishedHelper.waitForCallback(callCount);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "onPageFinished should be called only for main frame fragment navigations.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        Assert.assertEquals(
                "onPageFinished should be called only for main frame fragment navigations.",
                mSyncURL.getSpec(), onPageFinishedHelper.getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDidFinishNavigation() throws Throwable {
        GURL emptyUrl = GURL.emptyGURL();
        boolean isInMainFrame = true;
        boolean isErrorPage = false;
        boolean isSameDocument = true;
        boolean fragmentNavigation = false;
        boolean isRendererInitiated = false;
        TestAwContentsClient.DoUpdateVisitedHistoryHelper doUpdateVisitedHistoryHelper =
                mContentsClient.getDoUpdateVisitedHistoryHelper();

        int callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(emptyUrl, isInMainFrame, !isErrorPage, !isSameDocument,
                fragmentNavigation, isRendererInitiated, PageTransition.TYPED);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.",
                emptyUrl.getSpec(), doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(mExampleURL, isInMainFrame, isErrorPage, !isSameDocument,
                fragmentNavigation, isRendererInitiated, PageTransition.TYPED);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.",
                mExampleURL.getSpec(), doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(emptyUrl, isInMainFrame, isErrorPage, !isSameDocument,
                fragmentNavigation, isRendererInitiated, PageTransition.TYPED);
        simulateNavigation(mExampleURL, !isInMainFrame, isErrorPage, !isSameDocument,
                fragmentNavigation, isRendererInitiated, PageTransition.TYPED);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should only be called for the main frame.",
                callCount + 1, doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should only be called for the main frame.",
                emptyUrl.getSpec(), doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(mExampleURL, isInMainFrame, isErrorPage, isSameDocument,
                !fragmentNavigation, !isRendererInitiated, PageTransition.RELOAD);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should be called for reloads.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should be called for reloads.",
                mExampleURL.getSpec(), doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(true, doUpdateVisitedHistoryHelper.getIsReload());
    }

    private void simulateNavigation(GURL gurl, boolean isInMainFrame, boolean isErrorPage,
            boolean isSameDocument, boolean isFragmentNavigation, boolean isRendererInitiated,
            int transition) {
        NavigationHandle navigation = new NavigationHandle(0 /* navigationHandleProxy */, gurl,
                isInMainFrame, isSameDocument, isRendererInitiated);
        mWebContentsObserver.didStartNavigation(navigation);

        navigation.didFinish(gurl, isErrorPage, true /* hasCommitted */, isFragmentNavigation,
                false /* isDownload */, false /* isValidSearchFormUrl */, transition,
                0 /* errorCode*/, 200 /* httpStatusCode*/);
        mWebContentsObserver.didFinishNavigation(navigation);
    }
}
