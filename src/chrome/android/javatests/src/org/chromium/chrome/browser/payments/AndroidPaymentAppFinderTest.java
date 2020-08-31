// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** An integration test for the Android payment app finder. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MediumTest
public class AndroidPaymentAppFinderTest
        implements PaymentAppFactoryDelegate, PaymentAppFactoryParams {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    /** Simulates a package manager in memory. */
    private final MockPackageManagerDelegate mPackageManager = new MockPackageManagerDelegate();

    /** Downloads from the test server. */
    private static class TestServerDownloader extends PaymentManifestDownloader {
        private GURL mTestServerUrl;

        /**
         * @param uri The URL of the test server.
         */
        /* package */ void setTestServerUrl(GURL url) {
            assert mTestServerUrl == null : "Test server URL should be set only once";
            mTestServerUrl = url;
        }

        @Override
        public void downloadPaymentMethodManifest(
                Origin merchantOrigin, GURL methodName, ManifestDownloadCallback callback) {
            super.downloadPaymentMethodManifest(
                    merchantOrigin, substituteTestServerUrl(methodName), callback);
        }

        @Override
        public void downloadWebAppManifest(Origin paymentMethodManifestOrigin,
                GURL webAppManifestUrl, ManifestDownloadCallback callback) {
            super.downloadWebAppManifest(paymentMethodManifestOrigin,
                    substituteTestServerUrl(webAppManifestUrl), callback);
        }

        private GURL substituteTestServerUrl(GURL url) {
            GURL changedUrl =
                    new GURL(url.getSpec().replaceAll("https://", mTestServerUrl.getSpec()));
            if (!changedUrl.isValid()) {
                assert false;
                return null;
            }
            return changedUrl;
        }
    }

    private final TestServerDownloader mDownloader = new TestServerDownloader();

    private EmbeddedTestServer mServer;
    private List<PaymentApp> mPaymentApps;
    private boolean mAllPaymentAppsCreated;
    private Map<String, PaymentMethodData> mMethodData;
    private boolean mRequestShippingOrPayerContact;

    // PaymentAppFactoryDelegate implementation.
    @Override
    public PaymentAppFactoryParams getParams() {
        return this;
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onPaymentAppCreated(PaymentApp paymentApp) {
        mPaymentApps.add(paymentApp);
    }

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onPaymentAppCreationError(String errorMessage) {}

    // PaymentAppFactoryDelegate implementation.
    @Override
    public void onDoneCreatingPaymentApps(PaymentAppFactoryInterface unusedFactory) {
        mAllPaymentAppsCreated = true;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public WebContents getWebContents() {
        return mRule.getActivity().getCurrentWebContents();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public RenderFrameHost getRenderFrameHost() {
        return getWebContents().getMainFrame();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Map<String, PaymentDetailsModifier> getModifiers() {
        return Collections.unmodifiableMap(new HashMap<String, PaymentDetailsModifier>());
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Origin getPaymentRequestSecurityOrigin() {
        return PaymentManifestDownloader.createOpaqueOriginForTest();
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public String getTopLevelOrigin() {
        return "https://top.level.origin";
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public String getPaymentRequestOrigin() {
        return "https://payment.request.origin";
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public Map<String, PaymentMethodData> getMethodData() {
        return mMethodData;
    }

    // PaymentAppFactoryParams implementation.
    @Override
    public boolean requestShippingOrPayerContact() {
        return mRequestShippingOrPayerContact;
    }

    @Before
    public void setUp() throws Throwable {
        mRule.startMainActivityOnBlankPage();
        mPackageManager.reset();
        mServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mDownloader.setTestServerUrl(new GURL(mServer.getURL("/components/test/data/payments/")));
        mPaymentApps = new ArrayList<>();
        mAllPaymentAppsCreated = false;
    }

    @After
    public void tearDown() {
        if (mServer != null) mServer.stopAndDestroyServer();
    }

    /** Absence of installed apps should result in no payment apps. */
    @Test
    @Feature({"Payments"})
    public void testNoApps() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        methods.add("https://alicepay.com/webpay");
        methods.add("https://bobpay.com/webpay");
        methods.add("https://charliepay.com/webpay");
        methods.add("https://davepay.com/webpay");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /** Payment apps without metadata should be filtered out. */
    @Test
    @Feature({"Payments"})
    public void testNoMetadata() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", null /* no metadata */, /*signature=*/"01");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        assertPaymentAppsCreated(/*no identifier*/);
    }

    /**
     * Payment apps without default payment method name in metadata should still be able to use
     * non-URL payment method names.
     */
    @Test
    @Feature({"Payments"})
    public void testNoDefaultPaymentMethodNameWithNonUrlPaymentMethodName() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay",
                "" /* no default payment method name in metadata */, /*signature=*/"AA");
        mPackageManager.setStringArrayMetaData("com.alicepay", new String[] {"basic-card"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals(1, mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals(
                "basic-card", mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should still match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals(1, mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals(
                "basic-card", mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());
    }

    /**
     * Payment apps cannot use a payment method without explicit authorization.
     */
    @Test
    @Feature({"Payments"})
    public void testPaymentAppsRequireExplicitAuthorizationForPaymentMethods() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://frankpay.com/webpay");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay",
                "" /* no default payment method name in metadata */, /*signature=*/"AA");
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://frankpay.com/webpay"});

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertTrue("No apps should still match the query", mPaymentApps.isEmpty());
    }

    /** Payment apps without a human-readable name should be filtered out. */
    @Test
    @Feature({"Payments"})
    public void testEmptyLabel() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        mPackageManager.installPaymentApp("" /* empty label */, "com.bobpay", "basic-card",
                /*signature=*/"01020304050607080900");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /** Invalid and relative URLs cannot be used as payment method names. */
    @Test
    @Feature({"Payments"})
    public void testInvalidPaymentMethodNames() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://"); // Invalid URL.
        methods.add("../index.html"); // Relative URL.
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", "https://", /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp(
                "AlicePay", "com.alicepay", "../index.html", /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /** Non-URL payment method names are hard-coded to those defined in W3C. */
    @Test
    @Feature({"Payments"})
    public void testTwoAppsWithIncorrectMethodNames() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        methods.add("incorrect-method-name"); // Even if merchant supports it, Chrome filters out
        // unknown non-URL method names.
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "incorrect-method-name",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "incorrect-method-name",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /**
     * Test "basic-card" payment method with a payment app that supports IS_READY_TO_PAY service.
     * Another non-payment app also supports IS_READY_TO_PAY service, but it should be filtered
     * out, because it's not a payment app.
     */
    @Test
    @Feature({"Payments"})
    public void testOneBasicCardAppWithAFewIsReadyToPayServices() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", "basic-card", /*signature=*/"01020304050607080900");
        mPackageManager.addIsReadyToPayService("com.bobpay");
        mPackageManager.addIsReadyToPayService("com.alicepay");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Test BobPay with https://bobpay.com/webpay payment method name, which the payment app
     * supports through the "default_applications" directive in the
     * https://bobpay.com/payment-manifest.json file. BobPay has the correct signature that
     * matches the fingerprint in https://bobpay.com/app.json.
     */
    @Test
    @Feature({"Payments"})
    public void testOneUrlMethodNameApp() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
    }

    /** When Chrome is not running in TWA, the app store billing methods should be filtered out. */
    @Test
    @Feature({"Payments"})
    public void testIgnoreAppStoreMethodsInNonTwa() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");

        addAppStoreMethodAndFindApps(/*appStorePackageName=*/"com.bobpay",
                /*appStorePaymentMethod=*/new GURL("https://bobpay.com/webpay"), methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /**
     * Test BobPay with an incorrect signature and https://bobpay.com/webpay payment method name.
     */
    @Test
    @Feature({"Payments"})
    public void testOneUrlMethodNameAppWithWrongSignature() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                "AA" /* incorrect signature */);

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /** A payment app whose package info cannot be retrieved should be filtered out. */
    @Test
    @Feature({"Payments"})
    public void testOneAppWithoutPackageInfo() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", "https://bobpay.com/webpay", null /* no package info*/);

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /** Unsigned payment app should be filtered out. */
    @Test
    @Feature({"Payments"})
    public void testOneAppWithoutSignatures() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", "https://bobpay.com/webpay", "" /* no signatures */);

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());
    }

    /**
     * If two payment apps both support "basic-card" payment method name, then they both should be
     * found.
     */
    @Test
    @Feature({"Payments"})
    public void testTwoBasicCardApps() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        mPackageManager.installPaymentApp(
                "BobPay", "com.bobpay", "basic-card", /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp(
                "AlicePay", "com.alicepay", "basic-card", /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("2 apps should match the query", 2, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));
    }

    /**
     * Test https://davepay.com/webpay payment method, the "default_applications" of which
     * supports two different package names: one for production and one for development version
     * of the payment app. Both of these apps should be found. Repeated lookups should continue
     * finding the two apps.
     */
    @Test
    @Feature({"Payments"})
    public void testTwoUrlMethodNameAppsWithSameMethodName() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://davepay.com/webpay");
        mPackageManager.installPaymentApp("DavePay", "com.davepay.prod",
                "https://davepay.com/webpay", /*signature=*/"44444444442222222222");
        mPackageManager.installPaymentApp("DavePay Dev", "com.davepay.dev",
                "https://davepay.com/webpay", /*signature=*/"44444444441111111111");

        findApps(methods);

        Assert.assertEquals("2 apps should match the query", 2, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.davepay.prod"));
        Assert.assertTrue(appIdentifiers.contains("com.davepay.dev"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("2 apps should match the query again", 2, mPaymentApps.size());
        appIdentifiers.clear();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.davepay.prod"));
        Assert.assertTrue(appIdentifiers.contains("com.davepay.dev"));
    }

    /**
     * If the merchant supports https://bobpay.com/webpay and https://alicepay.com/webpay payment
     * method names and the user has an app for each of those, then both apps should be found.
     * Repeated lookups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testTwoUrlMethodNameAppsWithDifferentMethodNames() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        methods.add("https://alicepay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("2 apps should match the query", 2, mPaymentApps.size());
        Set<String> appIdentifiers = new HashSet<>();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("2 apps should match the query again", 2, mPaymentApps.size());
        appIdentifiers.clear();
        appIdentifiers.add(mPaymentApps.get(0).getIdentifier());
        appIdentifiers.add(mPaymentApps.get(1).getIdentifier());
        Assert.assertTrue(appIdentifiers.contains("com.bobpay"));
        Assert.assertTrue(appIdentifiers.contains("com.alicepay"));
    }

    /**
     * If the merchant supports a couple of payment methods, one of which does not have a valid
     * manifest, then all apps that support the invalid manifest should be filtered out. Repeated
     * calls should continue finding only the payment app for the valid manifest.
     */
    @Test
    @Feature({"Payments"})
    public void testOneValidManifestAndOneInvalidManifestWithPaymentAppsForBoth() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        methods.add("https://not-valid.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("NotValid", "com.not-valid",
                "https://not-valid.com/webpay", /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should match the query again", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Repeated lookups of payment apps for URL method names should continue finding the same
     * payment apps.
     */
    @Test
    @Feature({"Payments"})
    public void testTwoUrlMethodNameAppsTwice() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        methods.add("https://alicepay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        assertPaymentAppsCreated("com.bobpay", "com.alicepay");

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        assertPaymentAppsCreated("com.bobpay", "com.alicepay");
    }

    /**
     * Test CharliePay Dev with https://charliepay.com/webpay payment method, which supports both
     * dev and prod versions of the app through multiple web app manifests. Repeated app look ups
     * should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testCharliePayDev() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://charliepay.com/webpay");
        mPackageManager.installPaymentApp("CharliePay", "com.charliepay.dev",
                "https://charliepay.com/webpay", /*signature=*/"33333333333111111111");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.charliepay.dev", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should match the query again", 1, mPaymentApps.size());
        Assert.assertEquals("com.charliepay.dev", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Test DavePay Dev with https://davepay.com/webpay payment method, which supports both
     * dev and prod versions of the app through multiple sections of "related_applications" entry
     * in the same web app manifest. Repeated app look ups should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testDavePayDev() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://davepay.com/webpay");
        mPackageManager.installPaymentApp("DavePay", "com.davepay.dev",
                "https://davepay.com/webpay", /*signature=*/"44444444441111111111");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.davepay.dev", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should match the query again", 1, mPaymentApps.size());
        Assert.assertEquals("com.davepay.dev", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Test a valid installation of EvePay with 55555555551111111111 signature and
     * https://evepay.com/webpay payment method, which supports a couple of different signatures
     * (with the same package name) through different web app manifests. Repeated app look ups
     * should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testValidEvePay1() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://evepay.com/webpay");
        mPackageManager.installPaymentApp("EvePay", "com.evepay", "https://evepay.com/webpay",
                /*signature=*/"55555555551111111111");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.evepay", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should match the query again", 1, mPaymentApps.size());
        Assert.assertEquals("com.evepay", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Test a valid installation of EvePay with 55555555552222222222 signature and
     * https://evepay.com/webpay payment method, which supports a couple of different signatures
     * (with the same package name) through different web app manifests. Repeated app look ups
     * should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testValidEvePay2() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://evepay.com/webpay");
        mPackageManager.installPaymentApp("EvePay", "com.evepay", "https://evepay.com/webpay",
                /*signature=*/"55555555552222222222");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.evepay", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should match the query again", 1, mPaymentApps.size());
        Assert.assertEquals("com.evepay", mPaymentApps.get(0).getIdentifier());
    }

    /**
     * Test an invalid installation of EvePay with https://evepay.com/webpay payment method, which
     * supports several different signatures (with the same package name) through different web app
     * manifests. Repeated app look ups should find no payment apps.
     */
    @Test
    @Feature({"Payments"})
    public void testInvalidEvePay() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://evepay.com/webpay");
        mPackageManager.installPaymentApp(
                "EvePay", "com.evepay", "https://evepay.com/webpay", /*signature=*/"55");

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertTrue("No apps should match the query again", mPaymentApps.isEmpty());
    }

    /**
     * Test https://frankpay.com/webpay payment method, which is invalid because it contains
     * {"supported_origins": "*"}. Repeated app look ups should find no payment apps.
     */
    @Test
    @Feature({"Payments"})
    public void testFrankPayDoesNotMatchAnyPaymentApps() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://frankpay.com/webpay");
        mPackageManager.installPaymentApp(
                "AlicePay", "com.alicepay", "https://alicepay.com/webpay", /*signature=*/"00");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "basic-card", /*signature=*/"11");
        mPackageManager.installPaymentApp(
                "AlicePay", "com.charliepay", "invalid-payment-method-name", /*signature=*/"22");
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://frankpay.com/webpay"});
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://frankpay.com/webpay"});
        mPackageManager.setStringArrayMetaData(
                "com.charliepay", new String[] {"https://frankpay.com/webpay"});

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        assertPaymentAppsCreated(/*no identifiers*/);
    }

    /**
     * Verify unable to use a payment app that has wrong signature for default
     * payment method and is not explicitly authorized to use any other method
     * either.
     */
    @Test
    @Feature({"Payments"})
    public void testInvalidSignatureAndPaymentMethodDoesNotMatchAnyPaymentApps() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://frankpay.com/webpay");
        methods.add("https://bobpay.com/webpay");
        methods.add("invalid-payment-method-name");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                "00" /* Invalid signature for https://bobpay.com/webpay. */);
        mPackageManager.setStringArrayMetaData("com.bobpay",
                new String[] {"invalid-payment-method-name", "https://frankpay.com/webpay"});

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertTrue("No apps should still match the query", mPaymentApps.isEmpty());
    }

    /**
     * Verify that only a valid AlicePay app can use https://georgepay.com/webpay payment method
     * name, because https://georgepay.com/payment-manifest.json contains {"supported_origins":
     * ["https://alicepay.com"]}. Repeated app look ups should be successful.
     */
    @Test
    @Feature({"Payments"})
    public void testGeorgePaySupportsPaymentAppsFromAlicePayOrigin() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://georgepay.com/webpay");
        // Valid AlicePay:
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://georgepay.com/webpay"});
        // Invalid AlicePay:
        mPackageManager.installPaymentApp("AlicePay", "com.fake-alicepay" /* invalid package name*/,
                "https://alicepay.com/webpay", "00" /* invalid signature */);
        mPackageManager.setStringArrayMetaData(
                "com.fake-alicepay", new String[] {"https://georgepay.com/webpay"});
        // Valid BobPay:
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://georgepay.com/webpay"});
        // A "basic-card" app.
        mPackageManager.installPaymentApp("CharliePay", "com.charliepay.dev", "basic-card",
                /*signature=*/"33333333333111111111");
        mPackageManager.setStringArrayMetaData(
                "com.charliepay.dev", new String[] {"https://georgepay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        assertPaymentAppsCreated("com.alicepay");
    }

    /**
     * Verify that AlicePay app with incorrect signature cannot use https://georgepay.com/webpay
     * payment method, which contains {"supported_origins": ["https://alicepay.com"]} in
     * https://georgepay.com/payment-manifest.json file. Repeated app look ups should find no apps.
     */
    @Test
    @Feature({"Payments"})
    public void testInvalidSignatureAlicePayAppCannotUseGeorgePayMethodName() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://georgepay.com/webpay");
        // AlicePay with invalid signature:
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                "00" /* invalid signature */);
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://georgepay.com/webpay"});

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertTrue("No apps should match the query again", mPaymentApps.isEmpty());
    }

    /**
     * Verify that BobPay app cannot use https://georgepay.com/webpay payment method, because
     * https://georgepay.com/payment-manifest.json contains
     * {"supported_origins": ["https://alicepay.com"]} and no "https://bobpay.com". BobPay can still
     * use its own payment method name, however. Repeated app look ups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testValidBobPayCannotUseGeorgePayMethod() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        methods.add("https://georgepay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://georgepay.com/webpay"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals("1 payment method should be enabled", 1,
                mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals("https://bobpay.com/webpay",
                mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertEquals("1 app should still match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals("1 payment method should still be enabled", 1,
                mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertEquals("https://bobpay.com/webpay",
                mPaymentApps.get(0).getInstrumentMethodNames().iterator().next());
    }

    /**
     * Verify that HenryPay can not use https://henrypay.com/webpay payment method name
     * and BobPay can not use it because https://henrypay.com/payment-manifest.json
     * contains invalid "supported_origins": "*". Repeated app look ups should
     * find no payment apps.
     */
    @Test
    @Feature({"Payments"})
    public void testUrlPaymentMethodWithDefaultApplication() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://henrypay.com/webpay");
        mPackageManager.installPaymentApp("HenryPay", "com.henrypay", "https://henrypay.com/webpay",
                /*signature=*/"55555555551111111111");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://henrypay.com/webpay"});

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        assertPaymentAppsCreated(/*no identifiers*/);
    }

    /**
     * Verify that no payment app can use https://henrypay.com/webpay, because it
     * does not explicitly authorize any payment app.
     */
    @Test
    @Feature({"Payments"})
    public void testNonUriDefaultPaymentMethodAppCanUseMethod() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://henrypay.com/webpay");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "basic-card", /*signature=*/"AA");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://henrypay.com/webpay"});

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        Assert.assertTrue("No apps should still match the query", mPaymentApps.isEmpty());
    }

    /**
     * Verify that IkePay can use https://ikepay.com/webpay payment method name because it's
     * a default application and AlicePay can use it because
     * https://ikepay.com/payment-manifest.json contains
     * "supported_origins": ["https://alicepay.com"]. BobPay cannot use this payment method.
     * Repeated app look ups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testUrlPaymentMethodWithDefaultApplicationAndOneSupportedOrigin() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://ikepay.com/webpay");
        mPackageManager.installPaymentApp("IkePay", "com.ikepay", "https://ikepay.com/webpay",
                /*signature=*/"66666666661111111111");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://ikepay.com/webpay"});
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://ikepay.com/webpay"});

        findApps(methods);

        assertPaymentAppsCreated("com.ikepay", "com.alicepay");

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        assertPaymentAppsCreated("com.ikepay", "com.alicepay");
    }

    /**
     * Verify that no payment app can use https://henrypay.com/webpay, because it
     * does not explicitly authorize any payment app.
     */
    @Test
    @Feature({"Payments"})
    public void testDuplicateDefaultAndSupportedMethod() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://henrypay.com/webpay");
        mPackageManager.installPaymentApp("HenryPay", "com.henrypay", "https://henrypay.com/webpay",
                /*signature=*/"55555555551111111111");
        mPackageManager.setStringArrayMetaData(
                "com.henrypay", new String[] {"https://henrypay.com/webpay"});

        findApps(methods);

        Assert.assertTrue("No apps should match the query", mPaymentApps.isEmpty());

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        assertPaymentAppsCreated(/*no identifiers*/);
    }

    /**
     * The basic test for {@link AndroidPaymentAppFinder#findAndroidPaymentApps} to find a app-store
     * (e.g., Google Store) billing app.
     */
    @Test
    @Feature({"Payments"})
    public void testFindAppStoreBillingApp() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://play.google.com/billing");
        mPackageManager.installPaymentApp("MerchantTwaApp", "com.merchant.twa",
                "https://play.google.com/billing", /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.merchant.twa", new String[] {"https://play.google.com/billing"});

        setMockTrustedWebActivity("com.merchant.twa");
        findApps(methods);

        assertPaymentAppsCreated("com.merchant.twa");
    }

    /**
     * For finding app store billing app, test scenario where no payment app has been installed. The
     * test setting intentionally omits the app installations.
     */
    @Test
    @Feature({"Payments"})
    public void testFindAppStoreBillingAppNoAppAvailable() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://play.google.com/billing");

        setMockTrustedWebActivity("com.merchant.twa");
        findApps(methods);

        assertPaymentAppsCreated(/*no identifiers*/);
    }

    /**
     * For finding app store billing app, test scenario where the app's meta data is null. The test
     * setting intentionally set the payment app's meta data to null.
     */
    @Test
    @Feature({"Payments"})
    public void testFindAppStoreBillingAppNullMetaData() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://play.google.com/billing");

        mPackageManager.installPaymentApp(
                "MerchantTwaApp", "com.merchant.twa", null, /*signature=*/"01020304050607080900");

        setMockTrustedWebActivity("com.merchant.twa");
        findApps(methods);

        assertPaymentAppsCreated(/*no identifiers*/);
    }

    /**
     * For finding app store billing app, test that the TWA only has default app store but no
     * support the billing method in its Android manifest. The test setting intentionally omits the
     * setting of the twa's supported methods.
     */
    @Test
    @Feature({"Payments"})
    public void testFindAppStoreBillingAppTwaHasDefaultAppStoreMethod() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://play.google.com/billing");
        mPackageManager.installPaymentApp("MerchantTwaApp", "com.merchant.twa",
                "https://play.google.com/billing", /*signature=*/"01020304050607080900");

        setMockTrustedWebActivity("com.merchant.twa");
        findApps(methods);

        assertPaymentAppsCreated("com.merchant.twa");
    }

    /**
     * For finding app store billing app, test that the TWA has support the billing method but no
     * default method in its manifest. The test setting intentionally set TWA's default method to a
     * non-store method.
     */
    @Test
    @Feature({"Payments"})
    public void testFindAppStoreBillingAppTwaHasSupportedAppStoreMethod() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://play.google.com/billing");
        mPackageManager.installPaymentApp("MerchantTwaApp", "com.merchant.twa", "an://invalid.url",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.merchant.twa", new String[] {"https://play.google.com/billing"});

        setMockTrustedWebActivity("com.merchant.twa");
        findApps(methods);

        assertPaymentAppsCreated("com.merchant.twa");
    }

    /**
     * For finding app store billing app, test that the TWA has support the billing method but no
     * default method in its manifest. The test setting intentionally set TWA's default method to a
     * non-store method.
     */
    @Test
    @Feature({"Payments"})
    public void testFindAppStoreBillingAppTwaCanSupportMultipleAppStoreMethods() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://play.google.com/billing");
        methods.add("https://another.appstore.com/billing");
        mPackageManager.installPaymentApp("MerchantTwaApp", "com.merchant.twa",
                "https://another.appstore.com/billing",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.merchant.twa", new String[] {"https://play.google.com/billing"});

        setMockTrustedWebActivity("com.merchant.twa");
        addAppStoreMethodAndFindApps(
                "com.another.appstore", new GURL("https://another.appstore.com/billing"), methods);

        assertPaymentAppsCreated("com.merchant.twa");
        assertPaymentAppHasMethods(mPaymentApps.get(0), "https://another.appstore.com/billing",
                "https://play.google.com/billing");
    }

    /**
     * For finding app store billing app, test that Chrome must be in TWA to use app store billing.
     * The test setting intentionally omits the twa mocking.
     */
    @Test
    @Feature({"Payments"})
    public void testFindAppStoreBillingAppMustInTwa() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://play.google.com/billing");
        methods.add("https://bobpay.com/webpay");

        mPackageManager.installPaymentApp("MerchantTwaApp", "com.merchant.twa",
                "https://play.google.com/billing", /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.merchant.twa", new String[] {"https://play.google.com/billing"});
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://bobpay.com/webpay"});

        findApps(methods);

        assertPaymentAppsCreated("com.bobpay");
    }

    /**
     * For finding app store billing app, test that the payment request must support the app store
     * billing method to be able to use it. The test setting intentionally omits the app store
     * billing method in the payment request.
     */
    @Test
    @Feature({"Payments"})
    public void testFindAppStoreBillingAppNotRequested() throws Throwable {
        Set<String> noRequestedMethod = new HashSet<>();
        noRequestedMethod.add("https://bobpay.com/webpay");

        mPackageManager.installPaymentApp("MerchantTwaApp", "com.merchant.twa",
                "https://play.google.com/billing", /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.merchant.twa", new String[] {"https://play.google.com/billing"});
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://bobpay.com/webpay"});

        setMockTrustedWebActivity("com.merchant.twa");
        findApps(noRequestedMethod);

        assertPaymentAppsCreated("com.bobpay");
    }

    /**
     * For finding app store billing app, test that Chrome can display payment apps of app-store
     * billing method and the normal (non-billing) payment method at the same time. The test setting
     * includes a normal native payment method and play billing method, and expects to see the
     * payment apps of both are displayed.
     */
    @Test
    @Feature({"Payments"})
    public void testFindAppStoreBillingAppInclusiveForBillingAndNonBilling() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://play.google.com/billing");
        methods.add("https://bobpay.com/webpay");
        mPackageManager.installPaymentApp("MerchantTwaApp", "com.merchant.twa",
                "https://play.google.com/billing", /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.merchant.twa", new String[] {"https://play.google.com/billing"});
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://bobpay.com/webpay"});

        setMockTrustedWebActivity("com.merchant.twa");
        findApps(methods);

        assertPaymentAppsCreated("com.merchant.twa", "com.bobpay");
    }

    /**
     * For finding app store billing app, test that if delegation is requested along with the
     * app-store billing method, the app-store billing method would be ignore. The test setting
     * requests the shipping or payer contact delegation.
     */
    @Test
    @Feature({"Payments"})
    public void testFindAppStoreBillingAppDelegationRejectBilling() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://play.google.com/billing");
        methods.add("https://bobpay.com/webpay");
        mRequestShippingOrPayerContact = true;
        mPackageManager.installPaymentApp("MerchantTwaApp", "com.merchant.twa",
                "https://play.google.com/billing",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.merchant.twa", new String[] {"https://play.google.com/billing"});
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://bobpay.com/webpay"});

        setMockTrustedWebActivity("com.merchant.twa");
        findApps(methods);

        assertPaymentAppsCreated("com.bobpay");
    }

    /**
     * If a payment method supports two apps from different origins, both apps should be found.
     * Repeated app look ups should succeed.
     */
    @Test
    @Feature({"Payments"})
    public void testTwoAppsFromDifferentOriginsWithTheSamePaymentMethod() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://jonpay.com/webpay");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay", "https://alicepay.com/webpay",
                /*signature=*/"ABCDEFABCDEFABCDEFAB");
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                /*signature=*/"01020304050607080900");
        mPackageManager.setStringArrayMetaData(
                "com.alicepay", new String[] {"https://jonpay.com/webpay"});
        mPackageManager.setStringArrayMetaData(
                "com.bobpay", new String[] {"https://jonpay.com/webpay"});

        findApps(methods);

        assertPaymentAppsCreated("com.alicepay", "com.bobpay");

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        assertPaymentAppsCreated("com.alicepay", "com.bobpay");
    }

    /**
     * All known payment method names are valid.
     */
    @Test
    @Feature({"Payments"})
    public void testAllKnownPaymentMethodNames() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("basic-card");
        methods.add("interledger");
        methods.add("payee-credit-transfer");
        methods.add("payer-credit-transfer");
        methods.add("tokenized-card");
        methods.add("not-supported");
        mPackageManager.installPaymentApp("AlicePay", "com.alicepay",
                "" /* no default payment method name in metadata */, /*signature=*/"AA");
        mPackageManager.setStringArrayMetaData("com.alicepay",
                new String[] {"basic-card", "interledger", "payee-credit-transfer",
                        "payer-credit-transfer", "tokenized-card", "not-supported"});

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.alicepay", mPaymentApps.get(0).getIdentifier());
        Assert.assertEquals(5, mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertTrue(mPaymentApps.get(0).getInstrumentMethodNames().contains("basic-card"));
        Assert.assertTrue(mPaymentApps.get(0).getInstrumentMethodNames().contains("interledger"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("payee-credit-transfer"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("payer-credit-transfer"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("tokenized-card"));

        mPaymentApps.clear();
        mAllPaymentAppsCreated = false;

        findApps(methods);

        assertPaymentAppsCreated("com.alicepay");
        Assert.assertEquals(5, mPaymentApps.get(0).getInstrumentMethodNames().size());
        Assert.assertTrue(mPaymentApps.get(0).getInstrumentMethodNames().contains("basic-card"));
        Assert.assertTrue(mPaymentApps.get(0).getInstrumentMethodNames().contains("interledger"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("payee-credit-transfer"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("payer-credit-transfer"));
        Assert.assertTrue(
                mPaymentApps.get(0).getInstrumentMethodNames().contains("tokenized-card"));
    }

    /**
     * Test BobPay with https://bobpay.com/webpay payment method name, and supported delegations
     */
    @Test
    @Feature({"Payments"})
    public void testPaymentAppWithSupportedDelegations() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        String[] supportedDelegations = {
                "shippingAddress", "payerName", "payerEmail", "payerPhone"};
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                supportedDelegations, /*signature=*/"01020304050607080900");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());

        // Verify supported delegations
        Assert.assertTrue(mPaymentApps.get(0).handlesShippingAddress());
        Assert.assertTrue(mPaymentApps.get(0).handlesPayerName());
        Assert.assertTrue(mPaymentApps.get(0).handlesPayerEmail());
        Assert.assertTrue(mPaymentApps.get(0).handlesPayerPhone());
    }

    /**
     * Test that Chrome should not crash because of invalid supported delegations
     */
    @Test
    @Feature({"Payments"})
    public void testPaymentAppWithInavalidDelegationValue() throws Throwable {
        Set<String> methods = new HashSet<>();
        methods.add("https://bobpay.com/webpay");
        String[] invalidDelegations = {"invalidDelegation"};
        mPackageManager.installPaymentApp("BobPay", "com.bobpay", "https://bobpay.com/webpay",
                invalidDelegations, /*signature=*/"01020304050607080900");

        findApps(methods);

        Assert.assertEquals("1 app should match the query", 1, mPaymentApps.size());
        Assert.assertEquals("com.bobpay", mPaymentApps.get(0).getIdentifier());

        // Verify that invalid delegation values are ignored.
        Assert.assertFalse(mPaymentApps.get(0).handlesShippingAddress());
        Assert.assertFalse(mPaymentApps.get(0).handlesPayerName());
        Assert.assertFalse(mPaymentApps.get(0).handlesPayerEmail());
        Assert.assertFalse(mPaymentApps.get(0).handlesPayerPhone());
    }

    private void findApps(Set<String> methodNames) throws Throwable {
        addAppStoreMethodAndFindApps(
                /*appStorePackageName=*/null, /*appStorePaymentMethod=*/null, methodNames);
    }

    private void addAppStoreMethodAndFindApps(String appStorePackageName,
            GURL appStorePaymentMethod, Set<String> methodNames) throws Throwable {
        mMethodData = buildMethodData(methodNames);
        mRule.runOnUiThread(() -> {
            AndroidPaymentAppFinder finder =
                    new AndroidPaymentAppFinder(new PaymentManifestWebDataService(), mDownloader,
                            new PaymentManifestParser(), mPackageManager,
                            /*delegate=*/AndroidPaymentAppFinderTest.this, /*factory=*/null);
            finder.bypassIsReadyToPayServiceInTest();
            if (appStorePackageName != null) {
                assert appStorePaymentMethod != null;
                assert appStorePaymentMethod.isValid();
                finder.addAppStoreForTest(appStorePackageName, appStorePaymentMethod);
            }
            finder.findAndroidPaymentApps();
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mAllPaymentAppsCreated;
            }
        });
    }

    private static Map<String, PaymentMethodData> buildMethodData(Set<String> methodNames) {
        Map<String, PaymentMethodData> result = new HashMap<>();
        for (String methodName : methodNames) {
            PaymentMethodData methodData = new PaymentMethodData();
            methodData.supportedMethod = methodName;
            result.put(methodName, methodData);
        }
        return result;
    }

    private void setMockTrustedWebActivity(String twaPackageName) {
        mPackageManager.setMockTrustedWebActivity(twaPackageName);
    }

    private void assertPaymentAppsCreated(String... expectedIds) {
        Set<String> ids = new HashSet<>();
        for (PaymentApp app : mPaymentApps) {
            ids.add(app.getIdentifier());
        }
        Assert.assertEquals(
                String.format(Locale.getDefault(), "Expected %d apps, but got %d apps instead.",
                        expectedIds.length, ids.size()),
                expectedIds.length, ids.size());
        for (String expectedId : expectedIds) {
            Assert.assertTrue(String.format(Locale.getDefault(),
                                      "Expected id %s is not found. "
                                              + "Expected identifiers: %s. "
                                              + "Actual identifiers: %s",
                                      expectedId, Arrays.toString(expectedIds), ids.toString()),
                    ids.contains(expectedId));
        }
    }

    private void assertPaymentAppHasMethods(PaymentApp app, String... expectedMethodNames) {
        Set<String> methodNames = app.getInstrumentMethodNames();
        Assert.assertEquals(String.format(Locale.getDefault(),
                                    "Expected %d methods, but got %d methods instead.",
                                    expectedMethodNames.length, methodNames.size()),
                expectedMethodNames.length, methodNames.size());
        for (String expectedId : expectedMethodNames) {
            Assert.assertTrue(String.format(Locale.getDefault(),
                                      "Expected method %s is not found. "
                                              + "Expected methods: %s. "
                                              + "Actual methods: %s",
                                      expectedId, Arrays.toString(expectedMethodNames),
                                      methodNames.toString()),
                    methodNames.contains(expectedId));
        }
    }
}
