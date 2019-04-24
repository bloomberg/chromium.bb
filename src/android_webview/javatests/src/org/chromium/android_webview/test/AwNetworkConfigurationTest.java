// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.net.URLEncoder;
import java.util.HashMap;
import java.util.Map;

/**
 * A test suite for WebView's network-related configuration. This tests WebView's default settings,
 * which are configured by either AwURLRequestContextGetter or NetworkContext.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwNetworkConfigurationTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testSHA1LocalAnchorsAllowed() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_SHA1_LEAF);
        try {
            CallbackHelper onReceivedSslErrorHelper = mContentsClient.getOnReceivedSslErrorHelper();
            int count = onReceivedSslErrorHelper.getCallCount();
            String url = mTestServer.getURL("/android_webview/test/data/hello_world.html");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            // TODO(ntfschr): update this assertion whenever
            // https://android.googlesource.com/platform/external/conscrypt/+/1d6a0b8453054b7dd703693f2ce2896ae061aee3
            // rolls into an Android release, as this will mean Android intends to distrust SHA1
            // (http://crbug.com/919749).
            Assert.assertEquals("We should not have received any SSL errors", count,
                    onReceivedSslErrorHelper.getCallCount());
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithHeaderMainFrame() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), echoHeaderUrl);
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            final String xRequestedWith = mActivityTestRule.getJavaScriptResultBodyTextContent(
                    mAwContents, mContentsClient);
            final String packageName = InstrumentationRegistry.getInstrumentation()
                                               .getTargetContext()
                                               .getPackageName();
            Assert.assertEquals("X-Requested-With header should be the app package name",
                    packageName, xRequestedWith);
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithHeaderSubResource() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            // Use the test server's root path as the baseUrl to satisfy same-origin restrictions.
            final String baseUrl = mTestServer.getURL("/");
            final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
            final String pageWithIframeHtml = "<html><body><p>Main frame</p><iframe src='"
                    + echoHeaderUrl + "'/></body></html>";
            // We use loadDataWithBaseUrlSync because we need to dynamically control the HTML
            // content, which EmbeddedTestServer doesn't support. We don't need to encode content
            // because loadDataWithBaseUrl() encodes content itself.
            mActivityTestRule.loadDataWithBaseUrlSync(mAwContents,
                    mContentsClient.getOnPageFinishedHelper(), pageWithIframeHtml,
                    /* mimeType */ null, /* isBase64Encoded */ false, baseUrl, /* historyUrl */
                    null);
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            final String xRequestedWith =
                    getJavaScriptResultIframeTextContent(mAwContents, mContentsClient);
            final String packageName = InstrumentationRegistry.getInstrumentation()
                                               .getTargetContext()
                                               .getPackageName();
            Assert.assertEquals("X-Requested-With header should be the app package name",
                    packageName, xRequestedWith);
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithHeaderHttpRedirect() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
            final String encodedEchoHeaderUrl = URLEncoder.encode(echoHeaderUrl, "UTF-8");
            // Returns a server-redirect (301) to echoHeaderUrl.
            final String redirectToEchoHeaderUrl =
                    mTestServer.getURL("/server-redirect?" + encodedEchoHeaderUrl);
            mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                    redirectToEchoHeaderUrl);
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            final String xRequestedWith = mActivityTestRule.getJavaScriptResultBodyTextContent(
                    mAwContents, mContentsClient);
            final String packageName = InstrumentationRegistry.getInstrumentation()
                                               .getTargetContext()
                                               .getPackageName();
            Assert.assertEquals("X-Requested-With header should be the app package name",
                    packageName, xRequestedWith);
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithApplicationValuePreferred() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            final String echoHeaderUrl = mTestServer.getURL("/echoheader?X-Requested-With");
            final String applicationRequestedWithValue = "foo";
            final Map<String, String> extraHeaders = new HashMap<>();
            extraHeaders.put("X-Requested-With", applicationRequestedWithValue);
            mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(),
                    echoHeaderUrl, extraHeaders);
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            final String xRequestedWith = mActivityTestRule.getJavaScriptResultBodyTextContent(
                    mAwContents, mContentsClient);
            Assert.assertEquals("Should prefer app's provided X-Requested-With header",
                    applicationRequestedWithValue, xRequestedWith);
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Network"})
    public void testRequestedWithHeaderShouldInterceptRequest() throws Throwable {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        try {
            final String url = mTestServer.getURL("/any-http-url-will-suffice.html");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            AwWebResourceRequest request =
                    mContentsClient.getShouldInterceptRequestHelper().getRequestsForUrl(url);
            Assert.assertFalse("X-Requested-With should be invisible to shouldInterceptRequest",
                    request.requestHeaders.containsKey("X-Requested-With"));
        } finally {
            mTestServer.stopAndDestroyServer();
        }
    }

    /**
     * Like {@link AwActivityTestRule#getJavaScriptResultBodyTextContent}, but it gets the text
     * content of the iframe instead. This assumes the main frame has only a single iframe.
     */
    private String getJavaScriptResultIframeTextContent(
            final AwContents awContents, final TestAwContentsClient viewClient) throws Exception {
        final String script =
                "document.getElementsByTagName('iframe')[0].contentDocument.body.textContent";
        String raw =
                mActivityTestRule.executeJavaScriptAndWaitForResult(awContents, viewClient, script);
        return mActivityTestRule.maybeStripDoubleQuotes(raw);
    }
}
