// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory.CustomTabNavigationDelegate;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation test for external navigation handling of a Custom Tab.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class CustomTabExternalNavigationTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    /**
     * A dummy activity that claims to handle "customtab://customtabtest".
     */
    public static class DummyActivityForSpecialScheme extends Activity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            finish();
        }
    }

    /**
     * A dummy activity that claims to handle "http://customtabtest.com".
     */
    public static class DummyActivityForHttp extends Activity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            finish();
        }
    }

    private static final String TWA_PACKAGE_NAME = "com.foo.bar";
    private static final String TEST_PATH = "/chrome/test/data/android/google.html";
    private CustomTabNavigationDelegate mNavigationDelegate;
    private EmbeddedTestServer mTestServer;
    private ExternalNavigationHandler mUrlHandler;

    @Before
    public void setUp() throws Exception {
        mCustomTabActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestServer = mCustomTabActivityTestRule.getTestServer();

        launchTwa(TWA_PACKAGE_NAME, mTestServer.getURL(TEST_PATH));
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        TabDelegateFactory delegateFactory = TabTestUtils.getDelegateFactory(tab);
        Assert.assertTrue(delegateFactory instanceof CustomTabDelegateFactory);
        CustomTabDelegateFactory customTabDelegateFactory =
                ((CustomTabDelegateFactory) delegateFactory);
        mUrlHandler = ThreadUtils.runOnUiThreadBlocking(
                () -> customTabDelegateFactory.createExternalNavigationHandler(tab));
        Assert.assertTrue(customTabDelegateFactory.getExternalNavigationDelegate()
                                  instanceof CustomTabNavigationDelegate);
        mNavigationDelegate = (CustomTabNavigationDelegate)
                                      customTabDelegateFactory.getExternalNavigationDelegate();
    }

    private void launchTwa(String twaPackageName, String url) throws TimeoutException {
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent(url);
        TrustedWebActivityTestUtil.spoofVerification(twaPackageName, url);
        TrustedWebActivityTestUtil.createSession(intent, twaPackageName);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    /**
     * For urls with special schemes and hosts, and there is exactly one activity having a matching
     * intent filter, the framework will make that activity the default handler of the special url.
     * This test tests whether chrome is able to start the default external handler.
     */
    @Test
    @SmallTest
    public void testExternalActivityStartedForDefaultUrl() {
        final String testUrl = "customtab://customtabtest/intent";
        ExternalNavigationParams params = new ExternalNavigationParams.Builder(testUrl, false)
                .build();
        @OverrideUrlLoadingResult
        int result = mUrlHandler.shouldOverrideUrlLoading(params);
        Assert.assertEquals(OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT, result);
        Assert.assertTrue("A dummy activity should have been started to handle the special url.",
                mNavigationDelegate.hasExternalActivityStarted());
    }

    /**
     * When loading a normal http url that chrome is able to handle, an intent picker should never
     * be shown, even if other activities such as {@link DummyActivityForHttp} claim to handle it.
     */
    @Test
    @SmallTest
    public void testIntentPickerNotShownForNormalUrl() {
        final String testUrl = "http://customtabtest.com";
        ExternalNavigationParams params = new ExternalNavigationParams.Builder(testUrl, false)
                .build();
        @OverrideUrlLoadingResult
        int result = mUrlHandler.shouldOverrideUrlLoading(params);
        Assert.assertEquals(OverrideUrlLoadingResult.NO_OVERRIDE, result);
        Assert.assertFalse("External activities should not be started to handle the url",
                mNavigationDelegate.hasExternalActivityStarted());
    }

    private @VerificationStatus int getCurrentPageVerifierStatus() {
        CustomTabActivity customTabActivity = mCustomTabActivityTestRule.getActivity();
        return customTabActivity.getComponent().resolveCurrentPageVerifier().getState().status;
    }

    /**
     * Tests that {@link CustomTabNavigationDelegate#shouldDisableExternalIntentRequestsForUrl()}
     * disables forwarding URL requests to external intents for navigations within the TWA's
     * origin.
     */
    @Test
    @SmallTest
    public void testShouldDisableExternalIntentRequestsForUrl() throws TimeoutException {
        String insideVerifiedOriginUrl =
                mTestServer.getURL("/chrome/test/data/android/simple.html");
        String outsideVerifiedOriginUrl = "https://example.com/test.html";

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(
                mCustomTabActivityTestRule.getActivity());
        Assert.assertEquals(VerificationStatus.SUCCESS, getCurrentPageVerifierStatus());

        Assert.assertTrue(mNavigationDelegate.shouldDisableExternalIntentRequestsForUrl(
                insideVerifiedOriginUrl));
        Assert.assertFalse(mNavigationDelegate.shouldDisableExternalIntentRequestsForUrl(
                outsideVerifiedOriginUrl));
    }
}
