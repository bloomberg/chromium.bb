// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"
#endif

namespace payments {
namespace {

struct ScopedTestSupport {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Invoking Play Billing on Chrome OS requires initializing the overlay
  // manager.
  ash::ArcOverlayManager overlay_manager;
#endif
};

class AndroidPaymentAppFactoryTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  AndroidPaymentAppFactoryTest() {
    feature_list_.InitAndEnableFeature(features::kAppStoreBilling);
  }

  ~AndroidPaymentAppFactoryTest() override = default;

  // PaymentRequestTestObserver:
  void OnUIDisplayed() override {
    PaymentRequestPlatformBrowserTestBase::OnUIDisplayed();
    FAIL() << "Browser UI should never be displayed for Android payment apps "
              "invoked here.";
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Even if a service worker app for app store payment method is installed, it
// should be ignored.
IN_PROC_BROWSER_TEST_F(AndroidPaymentAppFactoryTest,
                       IgnoreInstalledPlayBillingServiceWorker) {
  NavigateTo("a.com", "/payment_handler_installer.html");
  ASSERT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "install('alicepay.com/app1/app.js', "
                            "['https://play.google.com/billing'], false)"));
  NavigateTo("b.com", "/can_make_payment_checker.html");
  ASSERT_EQ("false", content::EvalJs(
                         GetActiveWebContents(),
                         "canMakePayment('https://play.google.com/billing')"));
}

// When an app store payment method app is available in a trusted web activity,
// then ignore other payment apps, since this is considered to be a digital
// goods purchase.
IN_PROC_BROWSER_TEST_F(AndroidPaymentAppFactoryTest,
                       IgnoreOtherPaymentAppsInTwaWhenHaveAppStoreBilling) {
  ScopedTestSupport scoped_test_support;
  (void)scoped_test_support;  // Avoid the "unused variable" warning.

  std::string method_name = https_server()->GetURL("a.com", "/").spec();
  method_name = method_name.substr(0, method_name.length() - 1);
  ASSERT_NE('/', method_name[method_name.length() - 1]);
  NavigateTo("a.com", "/payment_handler_installer.html");
  ASSERT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace(
              "install('payment_request_success_responder.js', [$1], false)",
              method_name)));

  // The "payment_request_success_responder.js" always replies with "{status:
  // success}", so the |response| here has to be distinct.
  std::string response = "App store payment method app response for test.";
  test_controller()->SetTwaPackageName("com.example.app");
  test_controller()->SetTwaPaymentApp("https://play.google.com/billing",
                                      "{\"status\": \"" + response + "\"}");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string expected_response = response;
#else
  std::string expected_response = "success";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  NavigateTo("b.com", "/payment_handler_status.html");
  ASSERT_EQ(expected_response,
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "getStatusList(['https://play.google.com/billing', $1])",
                    method_name)));
}

// https://play.google.com/billing payment method.
IN_PROC_BROWSER_TEST_F(AndroidPaymentAppFactoryTest, PlayBillingPaymentMethod) {
  ScopedTestSupport scoped_test_support;
  (void)scoped_test_support;  // Avoid the "unused variable" warning.

  std::string response = "App store payment method app response for test.";
  test_controller()->SetTwaPackageName("com.example.app");
  test_controller()->SetTwaPaymentApp("https://play.google.com/billing",
                                      "{\"status\": \"" + response + "\"}");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string expected_response = response;
#else
  std::string expected_response =
      "The payment method \"https://play.google.com/billing\" is not "
      "supported.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  NavigateTo("b.com", "/payment_handler_status.html");
  ASSERT_EQ(expected_response,
            content::EvalJs(GetActiveWebContents(),
                            "getStatus('https://play.google.com/billing')"));
}

// Passing a promise into PaymentRequest.show() should skip browser sheet with
// https://play.google.com/billing payment method.
IN_PROC_BROWSER_TEST_F(AndroidPaymentAppFactoryTest,
                       ShowPromiseShouldSkipBrowserPaymentSheet) {
  ScopedTestSupport scoped_test_support;
  (void)scoped_test_support;  // Avoid the "unused variable" warning.

  std::string response = "App store payment method app response for test.";
  test_controller()->SetTwaPackageName("com.example.app");
  test_controller()->SetTwaPaymentApp("https://play.google.com/billing",
                                      "{\"status\": \"" + response + "\"}");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string expected_response = response;
#else
  std::string expected_response =
      "The payment method \"https://play.google.com/billing\" is not "
      "supported.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  NavigateTo("b.com", "/payment_handler_status.html");
  ASSERT_EQ(expected_response,
            content::EvalJs(
                GetActiveWebContents(),
                "getStatusForMethodDataWithShowPromise([{supportedMethods:"
                "'https://play.google.com/billing'}])"));
}

// PaymentRequest.show(Promise.resolve({})) should skip browser sheet with
// https://play.google.com/billing payment method.
IN_PROC_BROWSER_TEST_F(AndroidPaymentAppFactoryTest,
                       EmptyShowPromiseShouldSkipBrowserPaymentSheet) {
  ScopedTestSupport scoped_test_support;
  (void)scoped_test_support;  // Avoid the "unused variable" warning.

  std::string response = "App store payment method app response for test.";
  test_controller()->SetTwaPackageName("com.example.app");
  test_controller()->SetTwaPaymentApp("https://play.google.com/billing",
                                      "{\"status\": \"" + response + "\"}");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string expected_response = response;
#else
  std::string expected_response =
      "The payment method \"https://play.google.com/billing\" is not "
      "supported.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  NavigateTo("b.com", "/payment_handler_status.html");
  ASSERT_EQ(expected_response,
            content::EvalJs(
                GetActiveWebContents(),
                "getStatusForMethodDataWithEmptyShowPromise([{supportedMethods:"
                "'https://play.google.com/billing'}])"));
}

}  // namespace
}  // namespace payments
