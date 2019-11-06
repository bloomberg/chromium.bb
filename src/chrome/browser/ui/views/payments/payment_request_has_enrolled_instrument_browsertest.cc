// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/payments/core/features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestHasEnrolledInstrumentQueryTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestHasEnrolledInstrumentQueryTest() {
    feature_list_.InitAndEnableFeature(
        ::features::kPaymentRequestHasEnrolledInstrument);
  }

  void SetUpOnMainThread() override {
    PaymentRequestBrowserTestBase::SetUpOnMainThread();

    // By default for these tests, can make payment is enabled. Individual tests
    // may override to false.
    SetCanMakePaymentEnabledPref(true);
  }

  void CallCanMakePayment() {
    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), "buy();"));
    WaitForObservedEvent();
  }

  void CallHasEnrolledInstrument() {
    ResetEventWaiterForSequence(
        {DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
         DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                       "hasEnrolledInstrument();"));
    WaitForObservedEvent();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestHasEnrolledInstrumentQueryTest);
};

// Visa is required, and user has a visa instrument.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryTest,
                       HasEnrolledInstrument) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"true"});
}

// Visa is required, and user has a visa instrument, but canMakePayment and
// hasEnrolledInstrument are disabled by user preference.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryTest,
                       HasEnrolledInstrumentButDisabled) {
  SetCanMakePaymentEnabledPref(false);
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"false"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"false"});
}

// Visa is required, and user has a masked visa instrument, and Google Pay cards
// in basic-card is disabled.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryTest,
                       HasEnrolledInstrument_GooglePayCardsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      payments::features::kReturnGooglePayInBasicCard);
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  card.SetNumber(base::ASCIIToUTF16("4111111111111111"));  // We need a visa.
  card.SetNetworkForMaskedCard(autofill::kVisaCard);
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"false"});
}

// Visa is required, and user has a masked visa instrument, and Google Pay cards
// in basic-card is enabled.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryTest,
                       HasEnrolledInstrument_GooglePayCardsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      payments::features::kReturnGooglePayInBasicCard);
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  autofill::CreditCard card = autofill::test::GetMaskedServerCard();
  card.SetNumber(base::ASCIIToUTF16("4111111111111111"));  // We need a visa.
  card.SetNetworkForMaskedCard(autofill::kVisaCard);
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"true"});
}

// Pages without a valid SSL certificate always get "false" from
// .canMakePayment() and .hasEnrolledInstrument().
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryTest,
                       HasEnrolledInstrument_InvalidSSL) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  SetInvalidSsl();

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"false"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"false"});
}

// Visa is required, user has a visa instrument, and user is in incognito
// mode.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryTest,
                       HasEnrolledInstrument_InIncognitoMode) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  SetIncognito();

  const autofill::CreditCard card = autofill::test::GetCreditCard();  // Visa.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"true"});
}

// Visa is required, and user doesn't have a visa instrument.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryTest,
                       HasEnrolledInstrument_NotSupported) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  const autofill::CreditCard card = autofill::test::GetCreditCard2();  // Amex.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"false"});
}

// Visa is required, user doesn't have a visa instrument and the user is in
// incognito mode. In this case canMakePayment returns false as in a normal
// profile.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryTest,
                       HasEnrolledInstrument_NotSupported_InIncognitoMode) {
  NavigateTo("/payment_request_can_make_payment_query_test.html");
  SetIncognito();

  const autofill::CreditCard card = autofill::test::GetCreditCard2();  // Amex.
  AddCreditCard(card);

  CallCanMakePayment();
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument();
  ExpectBodyContains({"false"});
}

class PaymentRequestHasEnrolledInstrumentQueryCCTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestHasEnrolledInstrumentQueryCCTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kPaymentRequestHasEnrolledInstrument);
  }

  // If |visa| is true, then the method data is:
  //
  //   [{supportedMethods: ['visa']}]
  //
  // If |visa| is false, then the method data is:
  //
  //   [{supportedMethods: ['mastercard']}]
  void CallCanMakePayment(bool visa) {
    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(),
                                       visa ? "buy();" : "other_buy();"));
    WaitForObservedEvent();
  }

  void CallHasEnrolledInstrument(bool visa) {
    ResetEventWaiterForSequence(
        {DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
         DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(
        GetActiveWebContents(), visa ? "hasEnrolledInstrument('visa');"
                                     : "hasEnrolledInstrument('mastercard');"));
    WaitForObservedEvent();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestHasEnrolledInstrumentQueryCCTest);
};

// Test that repeated canMakePayment and hasEnrolledInstrument queries are
// allowed when the payment method specifics don't change.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryCCTest,
                       HasEnrolledInstrumentQueryQuota) {
  NavigateTo("/payment_request_can_make_payment_query_cc_test.html");

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"false"});

  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"false"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  CallCanMakePayment(/*visa=*/true);
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"true"});

  CallCanMakePayment(/*visa=*/false);
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"true"});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryCCTest,
                       QueryQuota) {
  NavigateTo("/payment_request_can_make_payment_query_cc_test.html");

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"false"});

  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains({"NotAllowedError"});
}

// hasEnrolledInstrument behaves the same in incognito mode as in normal mode to
// avoid incognito mode detection.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryCCTest,
                       QueryQuotaInIncognito) {
  NavigateTo("/payment_request_can_make_payment_query_cc_test.html");
  SetIncognito();

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"false"});

  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // visa

  CallHasEnrolledInstrument(/*visa=*/true);
  ExpectBodyContains({"true"});

  CallHasEnrolledInstrument(/*visa=*/false);
  ExpectBodyContains({"NotAllowedError"});
}

class PaymentRequestHasEnrolledInstrumentQueryPMITest
    : public PaymentRequestBrowserTestBase {
 protected:
  enum class CheckFor {
    BASIC_VISA,
    BASIC_CARD,
    ALICE_PAY,
    BOB_PAY,
    BOB_PAY_AND_BASIC_CARD,
    BOB_PAY_AND_VISA,
  };

  PaymentRequestHasEnrolledInstrumentQueryPMITest() {
    script_[CheckFor::BASIC_VISA] = "[basicVisaMethod]";
    script_[CheckFor::BASIC_CARD] = "[basicCardMethod]";
    script_[CheckFor::ALICE_PAY] = "[alicePayMethod]";
    script_[CheckFor::BOB_PAY] = "[bobPayMethod]";
    script_[CheckFor::BOB_PAY_AND_BASIC_CARD] =
        "[bobPayMethod, basicCardMethod]";
    script_[CheckFor::BOB_PAY_AND_VISA] = "[bobPayMethod, basicVisaMethod]";
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kPaymentRequestHasEnrolledInstrument);
  }

  void CallCanMakePayment(CheckFor check_for) {
    ResetEventWaiterForSequence({DialogEvent::CAN_MAKE_PAYMENT_CALLED,
                                 DialogEvent::CAN_MAKE_PAYMENT_RETURNED});

    ASSERT_TRUE(content::ExecuteScript(
        GetActiveWebContents(),
        "checkCanMakePayment(" + script_[check_for] + ");"));
    WaitForObservedEvent();
  }

  void CallHasEnrolledInstrument(CheckFor check_for) {
    ResetEventWaiterForSequence(
        {DialogEvent::HAS_ENROLLED_INSTRUMENT_CALLED,
         DialogEvent::HAS_ENROLLED_INSTRUMENT_RETURNED});
    ASSERT_TRUE(content::ExecuteScript(
        GetActiveWebContents(),
        "checkHasEnrolledInstrument(" + script_[check_for] + ");"));
    WaitForObservedEvent();
  }

 private:
  std::map<CheckFor, std::string> script_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestHasEnrolledInstrumentQueryPMITest);
};

// If the device does not have any payment apps installed, canMakePayment() and
// hasEnrolledInstrument() should return false for them.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryPMITest,
                       QueryQuotaForPaymentApps) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebPaymentsPerMethodCanMakePaymentQuota);
  NavigateTo("/payment_request_payment_method_identifier_test.html");

  CallCanMakePayment(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
}

// If the device does not have any payment apps installed,
// hasEnrolledInstrument() queries for both payment apps and
// basic-card depend only on what cards the user has on file.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryPMITest,
                       QueryQuotaForPaymentAppsAndCards) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard2());  // Amex

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // Visa

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});
}

// Querying for payment apps in incognito returns result as normal mode to avoid
// incognito mode detection. Multiple queries for different apps are rejected
// with NotSupportedError to avoid user fingerprinting.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryPMITest,
                       QueryQuotaForPaymentAppsInIncognitoMode) {
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enable_features=*/{::features::kServiceWorkerPaymentApps,
                           features::kWebPaymentsPerMethodCanMakePaymentQuota},
      /*disable_features=*/{});

  SetIncognito();

  CallCanMakePayment(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::ALICE_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
}

// Querying for both payment apps and autofill cards in incognito returns result
// as in normal mode to avoid incognito mode detection.
// Multiple queries for different payment methods are rejected with
// NotSupportedError to avoid user fingerprinting.
IN_PROC_BROWSER_TEST_F(PaymentRequestHasEnrolledInstrumentQueryPMITest,
                       NoQueryQuotaForPaymentAppsAndCardsInIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWebPaymentsPerMethodCanMakePaymentQuota);
  NavigateTo("/payment_request_payment_method_identifier_test.html");
  SetIncognito();

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard2());  // Amex

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_BASIC_CARD);
  ExpectBodyContains({"NotAllowedError"});

  AddCreditCard(autofill::test::GetCreditCard());  // Visa

  CallCanMakePayment(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY_AND_VISA);
  ExpectBodyContains({"true"});

  CallCanMakePayment(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});
  CallHasEnrolledInstrument(CheckFor::BOB_PAY);
  ExpectBodyContains({"false"});

  CallCanMakePayment(CheckFor::BASIC_VISA);
  ExpectBodyContains({"true"});
  CallHasEnrolledInstrument(CheckFor::BASIC_VISA);
  ExpectBodyContains({"true"});
}

}  // namespace payments
