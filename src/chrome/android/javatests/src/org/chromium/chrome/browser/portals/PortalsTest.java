// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.portals;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for the chrome/ layer support of the HTML portal element.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=Portals"})
public class PortalsTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    private class TabContentsSwapObserver extends EmptyTabObserver {
        private final CallbackHelper mCallbackHelper;

        public TabContentsSwapObserver() {
            mCallbackHelper = new CallbackHelper();
        }

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            mCallbackHelper.notifyCalled();
        }
    }

    private void executeScriptAndAwaitSwap(Tab tab, String code) throws Exception {
        TabContentsSwapObserver swapObserver = new TabContentsSwapObserver();
        CallbackHelper swapWaiter = swapObserver.getCallbackHelper();
        tab.addObserver(swapObserver);

        int currSwapCount = swapWaiter.getCallCount();
        JavaScriptUtils.executeJavaScript(tab.getWebContents(), code);
        swapWaiter.waitForCallback(currSwapCount, 1);
    }

    /**
     * Tests that a portal can be activated and have its contents swapped in to its embedder's tab.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testActivate() throws Exception {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(
                "/chrome/test/data/android/portals/portal-to-basic-content.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        final WebContents embedderContents = tab.getWebContents();
        Assert.assertNotNull(embedderContents);

        executeScriptAndAwaitSwap(tab, "activatePortal();");

        final WebContents portalContents = tab.getWebContents();
        Assert.assertNotNull(portalContents);
        Assert.assertNotSame(embedderContents, portalContents);
    }

    /**
     * Tests that a portal can be activated and adopt the embedder's contents.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testAdoptPredecessor() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        JavaScriptUtils.runJavascriptWithAsyncResult(tab.getWebContents(),
                "pingPredecessor().then(() => { domAutomationController.send(true); });");
    }

    /**
     * Tests that an adopted predecessor can be activated and adopt its portal back.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testReactivatePredecessor() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        executeScriptAndAwaitSwap(tab, "reactivatePredecessor();");
    }

    /**
     * Tests that an adopted predecessor can be destroyed.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testRemovePredecessor() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "removePredecessor();");
    }

    /**
     * Tests that a previously activated portal can be destroyed.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testRemovePreviouslyActivePortal() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        executeScriptAndAwaitSwap(tab, "reactivatePredecessor();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "removePortal();");
    }
}
