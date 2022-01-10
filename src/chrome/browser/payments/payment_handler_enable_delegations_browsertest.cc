// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"

namespace payments {
namespace {

class PaymentHandlerEnableDelegationsTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  PaymentHandlerEnableDelegationsTest() = default;

  PaymentHandlerEnableDelegationsTest(
      const PaymentHandlerEnableDelegationsTest&) = delete;
  PaymentHandlerEnableDelegationsTest& operator=(
      const PaymentHandlerEnableDelegationsTest&) = delete;

  ~PaymentHandlerEnableDelegationsTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/payment_handler.html");
  }
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerEnableDelegationsTest, EnableDelegations) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));
  EXPECT_EQ(
      expected,
      content::EvalJs(
          GetActiveWebContents(),
          "enableDelegations(["
          "'shippingAddress', 'payerName', 'payerPhone', 'payerEmail'])"));
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerEnableDelegationsTest,
                       InvalidDelegations) {
  std::string expected = "success";
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "install()"));

  expected =
      "TypeError: Failed to execute 'enableDelegations' on 'PaymentManager': "
      "The provided value 'invalid_delegation' is not a valid enum value of "
      "type PaymentDelegation.";
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            "enableDelegations(['invalid_delegation'])"));
}

}  // namespace
}  // namespace payments
