// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class IgnorePaymentMethodTest : public PaymentRequestPlatformBrowserTestBase {
 protected:
  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();

    method_name_ = https_server()->GetURL("a.com", "/nickpay.com/pay").spec();
    ASSERT_NE('/', method_name_[method_name_.length() - 1]);
  }

  void InstallTestPaymentHandler(const std::string& file_name) {
    NavigateTo("a.com", "/payment_handler_installer.html");
    ASSERT_EQ("success",
              content::EvalJs(GetActiveWebContents(),
                              content::JsReplace("install($1, [$2], false)",
                                                 file_name, method_name_)));
  }

  void VerifyFunctionOutput(const std::string& expected_return_value,
                            const std::string& function_name) {
    EXPECT_EQ(expected_return_value,
              content::EvalJs(GetActiveWebContents(),
                              content::JsReplace(function_name, method_name_)));
  }

  std::string method_name_;
};

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest, InstalledPHCannotMakePayments) {
  InstallTestPaymentHandler("can_make_payment_true_responder.js");
  NavigateTo("b.com", "/can_make_payment_checker.html");
  VerifyFunctionOutput("true", "canMakePayment($1)");

  ServiceWorkerPaymentAppFinder::GetInstance()->IgnorePaymentMethodForTest(
      method_name_);

  VerifyFunctionOutput("false", "canMakePayment($1)");
}

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest,
                       InstalledPHHasNoEnrolledInstruments) {
  InstallTestPaymentHandler("can_make_payment_true_responder.js");
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");
  VerifyFunctionOutput("true", "hasEnrolledInstrument($1)");

  ServiceWorkerPaymentAppFinder::GetInstance()->IgnorePaymentMethodForTest(
      method_name_);

  VerifyFunctionOutput("false", "hasEnrolledInstrument($1)");
}

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest, InstalledPHCannotBeLaunched) {
  InstallTestPaymentHandler("payment_request_success_responder.js");
  NavigateTo("b.com", "/payment_handler_status.html");
  VerifyFunctionOutput("success", "getStatus($1)");

  ServiceWorkerPaymentAppFinder::GetInstance()->IgnorePaymentMethodForTest(
      method_name_);

  VerifyFunctionOutput(
      "The payment method \"" + method_name_ + "\" is not supported.",
      "getStatus($1)");
}

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest,
                       JITInstallablePHCannotMakePayments) {
  NavigateTo("b.com", "/can_make_payment_checker.html");
  VerifyFunctionOutput("true", "canMakePayment($1)");

  ServiceWorkerPaymentAppFinder::GetInstance()->IgnorePaymentMethodForTest(
      method_name_);

  VerifyFunctionOutput("false", "canMakePayment($1)");
}

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest,
                       JITInstallablePHHasNoEnrolledInstruments) {
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");
  VerifyFunctionOutput(
#if defined(OS_ANDROID)
      // TODO(crbug.com/994799#c2): Android should return "false" for
      // hasEnrolledInstrument() of a JIT installable service worker payment
      // handler.
      "true",
#else
      "false",
#endif
      "hasEnrolledInstrument($1)");

  ServiceWorkerPaymentAppFinder::GetInstance()->IgnorePaymentMethodForTest(
      method_name_);

  VerifyFunctionOutput("false", "hasEnrolledInstrument($1)");
}

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest,
                       JITInstallablePHCanBeInstalledAndLaunchedByDefault) {
  NavigateTo("b.com", "/payment_handler_status.html");
  VerifyFunctionOutput("success", "getStatus($1)");
}

IN_PROC_BROWSER_TEST_F(
    IgnorePaymentMethodTest,
    JITInstallablePHCannotBeInstalledAndLaunchedWhenIgnored) {
  NavigateTo("b.com", "/payment_handler_status.html");

  ServiceWorkerPaymentAppFinder::GetInstance()->IgnorePaymentMethodForTest(
      method_name_);

  VerifyFunctionOutput(
      "The payment method \"" + method_name_ + "\" is not supported.",
      "getStatus($1)");
}

}  // namespace
}  // namespace payments
