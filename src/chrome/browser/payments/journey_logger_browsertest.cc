// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/journey_logger.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace payments {

class JourneyLoggerTest : public PaymentRequestPlatformBrowserTestBase {
 public:
  JourneyLoggerTest() : gpay_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~JourneyLoggerTest() override = default;

  void PreRunTestOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    main_frame_url_ = https_server()->GetURL("/journey_logger_test.html");
    ASSERT_TRUE(
        content::NavigateToURL(GetActiveWebContents(), main_frame_url_));
  }

  void SetUpForGpay() {
    gpay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/google.com/");
    ASSERT_TRUE(gpay_server_.Start());

    // Set up test manifest downloader that knows how to fake origin.
    const std::string method_name = "google.com";
    SetDownloaderAndIgnorePortInOriginComparisonForTesting(
        {{method_name, &gpay_server_}});

    gpay_scope_url_ = gpay_server_.GetURL("google.com", "/");
  }

  const GURL& main_frame_url() const { return main_frame_url_; }
  const GURL& gpay_scope_url() const { return gpay_scope_url_; }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

 private:
  net::EmbeddedTestServer gpay_server_;
  GURL main_frame_url_;
  GURL gpay_scope_url_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(JourneyLoggerTest);
};

IN_PROC_BROWSER_TEST_F(JourneyLoggerTest, NoPaymentMethodSupported) {
  base::HistogramTester histogram_tester;

  ResetEventWaiterForSingleEvent(TestEvent::kShowAppsReady);
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "testBasicCard()"));
  WaitForObservedEvent();

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "abort()"));

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
}

IN_PROC_BROWSER_TEST_F(JourneyLoggerTest, BasicCardOnly) {
  CreateAndAddCreditCardForProfile(CreateAndAddAutofillProfile());
  base::HistogramTester histogram_tester;

  ResetEventWaiterForSingleEvent(TestEvent::kShowAppsReady);
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "testBasicCard()"));
  WaitForObservedEvent();

  EXPECT_EQ(true, content::EvalJs(GetActiveWebContents(), "abort()"));

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
}

IN_PROC_BROWSER_TEST_F(JourneyLoggerTest, GooglePaymentApp) {
  base::HistogramTester histogram_tester;
  SetUpForGpay();

  EXPECT_EQ("{\"apiVersion\":1}",
            content::EvalJs(GetActiveWebContents(), "testGPay()"));

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
}

// Make sure the UKM was logged correctly.
IN_PROC_BROWSER_TEST_F(JourneyLoggerTest, UKMTransactionAmountRecorded) {
  SetUpForGpay();
  EXPECT_EQ("{\"apiVersion\":1}",
            content::EvalJs(GetActiveWebContents(), "testGPay()"));

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentRequest_TransactionAmount::kEntryName);
  size_t num_entries = entries.size();
  EXPECT_EQ(2u, num_entries);
  for (size_t i = 0; i < num_entries; i++) {
    test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[i], main_frame_url());
    EXPECT_EQ(2U, entries[i]->metrics.size());
    test_ukm_recorder()->ExpectEntryMetric(
        entries[i],
        ukm::builders::PaymentRequest_TransactionAmount::kCompletionStatusName,
        i != 0 /* completed */);
    test_ukm_recorder()->ExpectEntryMetric(
        entries[i],
        ukm::builders::PaymentRequest_TransactionAmount::kCategoryName,
        static_cast<int64_t>(
            JourneyLogger::TransactionSize::kRegularTransaction));
  }
}

IN_PROC_BROWSER_TEST_F(JourneyLoggerTest,
                       UKMCheckoutEventsRecordedForAppOrigin) {
  GURL merchant_url = https_server()->GetURL("/payment_handler.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), merchant_url));
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "install()"));

  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), "launch()"));
  WaitForObservedEvent();

  // UKM for merchant's website origin.
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentRequest_CheckoutEvents::kEntryName);
  size_t num_entries = entries.size();
  EXPECT_EQ(1u, num_entries);
  test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[0], merchant_url);

  // UKM for payment app's scope.
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentApp_CheckoutEvents::kEntryName);
  num_entries = entries.size();
  EXPECT_EQ(1u, num_entries);
  test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[0],
                                               https_server()->GetURL("/"));
}

IN_PROC_BROWSER_TEST_F(
    JourneyLoggerTest,
    UKMCheckoutEventsNotRecordedForAppOriginWhenNoWindowShown) {
  SetUpForGpay();

  EXPECT_EQ("{\"apiVersion\":1}",
            content::EvalJs(GetActiveWebContents(), "testGPay()"));

  // UKM for merchant's website origin.
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentRequest_CheckoutEvents::kEntryName);
  size_t num_entries = entries.size();
  EXPECT_EQ(1u, num_entries);
  test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[0], main_frame_url());

  // No UKM for payment app's scope since the app's origin is not shown inside
  // the PH modal window.
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentApp_CheckoutEvents::kEntryName);
  num_entries = entries.size();
  EXPECT_EQ(0u, num_entries);
}

IN_PROC_BROWSER_TEST_F(
    JourneyLoggerTest,
    UKMCheckoutEventsNotRecordedForAppOriginWhenNoAppInvoked) {
  CreateAndAddCreditCardForProfile(CreateAndAddAutofillProfile());

  ResetEventWaiterForSingleEvent(TestEvent::kShowAppsReady);
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "testBasicCard()"));
  WaitForObservedEvent();

  EXPECT_EQ(true, content::EvalJs(GetActiveWebContents(), "abort()"));

  // UKM for merchant's website origin.
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentRequest_CheckoutEvents::kEntryName);
  size_t num_entries = entries.size();
  EXPECT_EQ(1u, num_entries);
  test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[0], main_frame_url());

  // No UKM for payment app's scope since the request got aborted before
  // invoking a payment app.
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentApp_CheckoutEvents::kEntryName);
  num_entries = entries.size();
  EXPECT_EQ(0u, num_entries);
}

}  // namespace payments
