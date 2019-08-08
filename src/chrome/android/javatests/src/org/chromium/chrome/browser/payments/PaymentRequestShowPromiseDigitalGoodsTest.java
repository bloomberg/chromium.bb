// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DisableAnimationsTestRule;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for the show promise with digital goods.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestShowPromiseDigitalGoodsTest implements MainActivityStartCallback {
    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public PaymentRequestTestRule mRule =
            new PaymentRequestTestRule("show_promise/digital_goods.html", this);

    @Override
    public void onMainActivityStarted()
            throws InterruptedException, ExecutionException, TimeoutException {}

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDigitalGoodsFastApp()
            throws InterruptedException, ExecutionException, TimeoutException {
        mRule.installPaymentApp("basic-card", PaymentRequestTestRule.HAVE_INSTRUMENTS,
                PaymentRequestTestRule.IMMEDIATE_RESPONSE);
        mRule.openPage();
        mRule.executeJavaScriptAndWaitForResult("create();");
        mRule.triggerUIAndWait(mRule.getReadyToPay());

        Assert.assertEquals("USD $1.00", mRule.getOrderSummaryTotal());

        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());

        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testDigitalGoodsSlowApp()
            throws InterruptedException, ExecutionException, TimeoutException {
        mRule.installPaymentApp("basic-card", PaymentRequestTestRule.HAVE_INSTRUMENTS,
                PaymentRequestTestRule.DELAYED_RESPONSE, PaymentRequestTestRule.DELAYED_CREATION);
        mRule.openPage();
        mRule.executeJavaScriptAndWaitForResult("create();");
        mRule.triggerUIAndWait(mRule.getReadyToPay());

        Assert.assertEquals("USD $1.00", mRule.getOrderSummaryTotal());

        mRule.clickAndWait(R.id.button_primary, mRule.getDismissed());

        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSkipUIFastApp()
            throws InterruptedException, ExecutionException, TimeoutException {
        mRule.installPaymentApp("basic-card", PaymentRequestTestRule.HAVE_INSTRUMENTS,
                PaymentRequestTestRule.IMMEDIATE_RESPONSE);
        mRule.openPage();
        mRule.executeJavaScriptAndWaitForResult("create();");
        mRule.enableSkipUIForBasicCard();

        mRule.openPageAndClickNodeAndWait("buy", mRule.getDismissed());

        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSkipUISlowApp()
            throws InterruptedException, ExecutionException, TimeoutException {
        mRule.installPaymentApp("basic-card", PaymentRequestTestRule.HAVE_INSTRUMENTS,
                PaymentRequestTestRule.DELAYED_RESPONSE, PaymentRequestTestRule.DELAYED_CREATION);
        mRule.openPage();
        mRule.executeJavaScriptAndWaitForResult("create();");
        mRule.enableSkipUIForBasicCard();

        mRule.openPageAndClickNodeAndWait("buy", mRule.getDismissed());

        mRule.expectResultContains(new String[] {"\"total\":\"1.00\""});
    }
}
