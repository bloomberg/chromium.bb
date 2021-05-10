// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.digitalgoods;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.browserservices.TestTrustedWebActivityService.COMMAND_SET_RESPONSE;
import static org.chromium.chrome.browser.browserservices.TestTrustedWebActivityService.SET_RESPONSE_BUNDLE;
import static org.chromium.chrome.browser.browserservices.TestTrustedWebActivityService.SET_RESPONSE_NAME;
import static org.chromium.chrome.browser.browserservices.digitalgoods.AcknowledgeConverter.RESPONSE_ACKNOWLEDGE;

import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.trusted.TrustedWebActivityCallback;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.dependency_injection.ChromeAppComponent;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.payments.mojom.DigitalGoods.GetDetailsResponse;
import org.chromium.payments.mojom.ItemDetails;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the Digital Goods flow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        PaymentRequestTestRule.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
        "enable-blink-features=DigitalGoods"})
public class DigitalGoodsTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/digital_goods.html";
    private static final String TWA_SERVICE_SCOPE = "https://www.example.com/notifications";

    private EmbeddedTestServer mTestServer;
    private String mTestPage;
    private TrustedWebActivityClient mClient;

    @Before
    public void setUp() throws TimeoutException {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized();

        ChromeAppComponent component = ChromeApplication.getComponent();
        component.resolveTwaPermissionManager().addDelegateApp(
                Origin.createOrThrow(TWA_SERVICE_SCOPE), "org.chromium.chrome.tests.support");
        mClient = component.resolveTrustedWebActivityClient();

        // TWAs only work with HTTPS.
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        mTestPage = mTestServer.getURL(TEST_PAGE);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), mTestPage));
    }

    /**
     * Test that calling methods in JavaScript in the page gets correctly plumbed through to
     * DigitalGoodsFactoryImpl.
     */
    @Test
    @MediumTest
    @DisableIf.Device(type = {UiDisableIf.TABLET})
    public void javaImplConnected() throws TimeoutException {
        FakeDigitalGoods fake = new FakeDigitalGoods();
        DigitalGoodsFactoryImpl.setDigitalGoodsForTesting(fake);

        fake.addItem("id1", "Item 1", "desc", "GBP", "10");

        exec("populateDigitalGoodsService()");
        waitForNonNull("digitalGoodsService");
        exec("populateItemDetails(['id1'])");
        waitForNonNull("itemDetails");

        assertEquals("\"Item 1\"", exec("itemDetails[0].title"));
    }

    /**
     * Tests that plumbing between the {@link DigitalGoodsImpl} and the TrustedWebActivityService
     * in the TWA is working.
     */
    @Test
    @MediumTest
    public void twaServiceConnected() throws TimeoutException {
        DigitalGoodsImpl impl = createFixedDigitalGoods();

        setTwaServiceResponse(GetDetailsConverter.RESPONSE_COMMAND,
                GetDetailsConverter.createResponseBundle(0,
                        GetDetailsConverter.createItemDetailsBundle(
                                "id1", "Item 1", "Desc 1", "GBP", "10")));

        CallbackHelper helper = new CallbackHelper();
        impl.getDetails(new String[] { "id1" }, new GetDetailsResponse() {
            @Override
            public void call(Integer responseCode, ItemDetails[] details) {
                assertEquals(0, responseCode.intValue());
                assertEquals("id1", details[0].itemId);
                assertEquals("Item 1", details[0].title);
                assertEquals("Desc 1", details[0].description);
                assertEquals("GBP", details[0].price.currency);
                assertEquals("10", details[0].price.value);
                helper.notifyCalled();
            }
        });
        helper.waitForFirst();
    }

    /**
     * Tests that calling JavaScript methods correctly navigates all the way through to the TWA.
     */
    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.LOLLIPOP_MR1,
            sdk_is_less_than = Build.VERSION_CODES.N)
    @DisableIf.Device(type = {UiDisableIf.TABLET})
    public void
    jsToTwaConnected() throws TimeoutException {
        DigitalGoodsFactoryImpl.setDigitalGoodsForTesting(createFixedDigitalGoods());

        // Note: The response code much be 0 for success otherwise it doesn't propagate through to
        // JS.
        setTwaServiceResponse(GetDetailsConverter.RESPONSE_COMMAND,
                GetDetailsConverter.createResponseBundle(0,
                        GetDetailsConverter.createItemDetailsBundle(
                                "id1", "Item 1", "Desc 1", "GBP", "10")));

        exec("populateDigitalGoodsService()");
        waitForNonNull("digitalGoodsService");
        exec("populateItemDetails(['id1'])");
        waitForNonNull("itemDetails");

        assertEquals("\"Item 1\"", exec("itemDetails[0].title"));
    }

    /**
     * Tests that acknowledge works correctly.
     */
    @Test
    @MediumTest
    public void acknowledge() throws TimeoutException {
        DigitalGoodsFactoryImpl.setDigitalGoodsForTesting(createFixedDigitalGoods());

        setTwaServiceResponse(RESPONSE_ACKNOWLEDGE, AcknowledgeConverter.createResponseBundle(0));

        exec("populateDigitalGoodsService()");
        waitForNonNull("digitalGoodsService");
        exec("callAcknowledge('sku', 'onetime')");
        waitForNonNull("acknowledgeFlag");
    }

    /**
     * Tests that acknowledge throws when given a non-zero response code.
     */
    @Test
    @MediumTest
    public void acknowledge_failsOnNonZeroResponse() throws TimeoutException {
        DigitalGoodsFactoryImpl.setDigitalGoodsForTesting(createFixedDigitalGoods());

        setTwaServiceResponse(RESPONSE_ACKNOWLEDGE, AcknowledgeConverter.createResponseBundle(1));

        exec("populateDigitalGoodsService()");
        waitForNonNull("digitalGoodsService");
        exec("callAcknowledge('sku', 'onetime')");
        waitForNonNull("acknowledgeError");
    }

    private DigitalGoodsImpl createFixedDigitalGoods() {
        // There is one bit of production code that we need to change to make these tests work.
        // The page executing the JavaScript will be running in the EmbeddedTestServer and will
        // therefore have an origin like localhost:12345 which won't match the intent filter that
        // has been set up for the TestTrustedWebActivityService.

        // To work around this, we create our own DigitalGoodsImpl with a custom Delegate that
        // provides the URL we want to see.
        DigitalGoodsImpl.Delegate delegate = () -> new GURL(TWA_SERVICE_SCOPE);
        DigitalGoodsAdapter adapter = new DigitalGoodsAdapter(mClient);
        return new DigitalGoodsImpl(adapter, delegate);
    }

    private void setTwaServiceResponse(String name, Bundle args) throws TimeoutException {
        // TestTrustedWebActivityService doesn't currently depend on any Chromium code, so instead
        // of dragging in a lot of dependencies, we use this method to set what its response will
        // be. This also keeps the TestTrustedWebActivityService simpler.
        Bundle response = new Bundle();
        response.putString(SET_RESPONSE_NAME, name);
        response.putBundle(SET_RESPONSE_BUNDLE, args);

        final CallbackHelper helper = new CallbackHelper();

        mClient.connectAndExecute(Uri.parse(TWA_SERVICE_SCOPE),
                (origin1, service) -> service.sendExtraCommand(COMMAND_SET_RESPONSE, response,
                        new TrustedWebActivityCallback() {
                            @Override
                            public void onExtraCallback(@NonNull String callbackName,
                                    @Nullable Bundle args) {
                                helper.notifyCalled();
                            }
                        }));
        helper.waitForFirst();
    }

    private void waitForNonNull(String variable) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            try {
                Assert.assertNotEquals("null", exec(variable));
            } catch (TimeoutException e) {
                Assert.fail();
            }
        });
    }

    private String exec(String command) throws TimeoutException {
        return mCustomTabActivityTestRule.runJavaScriptCodeInCurrentTab(command);
    }
}
