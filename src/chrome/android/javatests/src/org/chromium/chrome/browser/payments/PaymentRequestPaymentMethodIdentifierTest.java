// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.DECEMBER;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.FIRST_BILLING_ADDRESS;
import static org.chromium.chrome.browser.payments.PaymentRequestTestRule.NEXT_YEAR;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.MainActivityStartCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for supported payment methods.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestPaymentMethodIdentifierTest implements MainActivityStartCallback {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_payment_method_identifier_test.html", this);

    @Before
    public void setUp() {
        PaymentRequestImpl.setIsLocalCanMakePaymentQueryQuotaEnforcedForTest();
    }

    @Override
    public void onMainActivityStarted() throws InterruptedException, ExecutionException,
            TimeoutException {
        // The user has a valid "visa" card.
        AutofillTestHelper helper = new AutofillTestHelper();
        String billingAddressId = helper.setProfile(new AutofillProfile("", "https://example.com",
                true, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles", "", "90291", "",
                "US", "555-555-5555", "", "en-US"));
        helper.setCreditCard(new CreditCard("", "https://example.com", true, true, "Jon Doe",
                "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */));
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCanPayWithBasicCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "checkBasicCard", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});

        mPaymentRequestTestRule.clickNodeAndWait(
                "buyBasicCard", mPaymentRequestTestRule.getReadyForInput());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testIgnoreCardType()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "checkBasicDebit", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});

        mPaymentRequestTestRule.clickNodeAndWait(
                "buyBasicDebit", mPaymentRequestTestRule.getReadyForInput());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testCannotMakeActivePaymentWithBasicMasterCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "checkBasicMasterCard", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"false"});

        mPaymentRequestTestRule.reTriggerUIAndWait(
                "buyBasicMasterCard", mPaymentRequestTestRule.getReadyForInput());
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSupportedNetworksMustMatchForCanMakePayment()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "checkBasicVisa", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});

        mPaymentRequestTestRule.clickNodeAndWait(
                "checkBasicMasterCard", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Not allowed to check whether can make payment"});

        mPaymentRequestTestRule.clickNodeAndWait(
                "checkBasicVisa", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});
    }

    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSupportedTypesMustMatchForCanMakePayment()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "checkBasicVisa", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});

        mPaymentRequestTestRule.clickNodeAndWait(
                "checkBasicDebit", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Not allowed to check whether can make payment"});

        mPaymentRequestTestRule.clickNodeAndWait(
                "checkBasicVisa", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});
    }

    /**
     * If the merchant requests supported methods of "mastercard" and "basic-card" with "visa"
     * network support, then the user should be able to pay via their "visa" card. The merchant will
     * receive the "basic-card" method name.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPayWithBasicCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.openPageAndClickNodeAndWait(
                "checkBasicVisa", mPaymentRequestTestRule.getCanMakePaymentQueryResponded());
        mPaymentRequestTestRule.expectResultContains(new String[] {"true"});

        mPaymentRequestTestRule.reTriggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogView.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Jon Doe", "4111111111111111", "12", "2050", "basic-card", "123"});
    }

    /**
     * If the merchant requests supported methods of "mastercard" and "basic-card" with "visa"
     * network support, then the user should be able to add a "mastercard" card and pay with it. The
     * merchant will receive the "mastercard" method name.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddMasterCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"5555-5555-5555-4444", "Jane Jones"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                mPaymentRequestTestRule.getBillingAddressChangeProcessed());
        mPaymentRequestTestRule.clickInCardEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogView.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"5555555555554444", "12", "Jane Jones", "123", "mastercard"});
    }

    /**
     * If the merchant requests supported methods of "mastercard" and "basic-card" with "visa"
     * network support, then the user should be able to add a new "visa" card and pay with it. The
     * merchant will receive the "basic-card" method name.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddBasicCard()
            throws InterruptedException, ExecutionException, TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait(mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInPaymentMethodAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInCardEditorAndWait(
                new String[] {"4242-4242-4242-4242", "Jane Jones"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.setSpinnerSelectionsInCardEditorAndWait(
                new int[] {DECEMBER, NEXT_YEAR, FIRST_BILLING_ADDRESS},
                mPaymentRequestTestRule.getBillingAddressChangeProcessed());
        mPaymentRequestTestRule.clickInCardEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getReadyForUnmaskInput());
        mPaymentRequestTestRule.setTextInCardUnmaskDialogAndWait(
                R.id.card_unmask_input, "123", mPaymentRequestTestRule.getReadyToUnmask());
        mPaymentRequestTestRule.clickCardUnmaskButtonAndWait(
                ModalDialogView.ButtonType.POSITIVE, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"4242424242424242", "12", "Jane Jones", "123", "basic-card"});
    }
}
