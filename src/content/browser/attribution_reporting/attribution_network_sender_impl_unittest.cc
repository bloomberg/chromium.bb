// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_network_sender_impl.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;

using Checkpoint = ::testing::MockFunction<void(int)>;

const char kReportUrl[] =
    "https://report.test/.well-known/attribution-reporting/report-attribution";

AttributionReport DefaultReport() {
  return ReportBuilder(
             AttributionInfoBuilder(SourceBuilder(base::Time()).BuildStored())
                 .Build())
      .Build();
}

}  // namespace

class AttributionNetworkSenderTest : public testing::Test {
 public:
  AttributionNetworkSenderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        network_sender_(std::make_unique<AttributionNetworkSenderImpl>(
            /*storage_partition=*/nullptr)),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    network_sender_->SetURLLoaderFactoryForTesting(shared_url_loader_factory_);
  }

 protected:
  // |task_environment_| must be initialized first.
  content::BrowserTaskEnvironment task_environment_;

  base::MockCallback<base::OnceCallback<void(AttributionReport, SendResult)>>
      callback_;

  // Unique ptr so it can be reset during testing.
  std::unique_ptr<AttributionNetworkSenderImpl> network_sender_;
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(AttributionNetworkSenderTest,
       ConversionReportReceived_NetworkRequestMade) {
  auto report = DefaultReport();
  network_sender_->SendReport(report, base::DoNothing());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, ""));
}

TEST_F(AttributionNetworkSenderTest, LoadFlags) {
  auto report = DefaultReport();
  network_sender_->SendReport(report, base::DoNothing());
  int load_flags =
      test_url_loader_factory_.GetPendingRequest(0)->request.load_flags;
  EXPECT_TRUE(load_flags & net::LOAD_BYPASS_CACHE);
  EXPECT_TRUE(load_flags & net::LOAD_DISABLE_CACHE);
}

TEST_F(AttributionNetworkSenderTest, Isolation) {
  auto report = DefaultReport();
  network_sender_->SendReport(report, base::DoNothing());
  network_sender_->SendReport(report, base::DoNothing());

  const network::ResourceRequest& request1 =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  const network::ResourceRequest& request2 =
      test_url_loader_factory_.GetPendingRequest(1)->request;

  EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
            request1.trusted_params->isolation_info.request_type());
  EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
            request2.trusted_params->isolation_info.request_type());

  EXPECT_TRUE(request1.trusted_params->isolation_info.network_isolation_key()
                  .IsTransient());
  EXPECT_TRUE(request2.trusted_params->isolation_info.network_isolation_key()
                  .IsTransient());

  EXPECT_NE(request1.trusted_params->isolation_info.network_isolation_key(),
            request2.trusted_params->isolation_info.network_isolation_key());
}

TEST_F(AttributionNetworkSenderTest, ReportSent_ReportBodySetCorrectly) {
  const struct {
    CommonSourceInfo::SourceType source_type;
    const char* expected_report;
  } kTestCases[] = {
      {CommonSourceInfo::SourceType::kNavigation,
       R"({"attribution_destination":"https://conversion.test",)"
       R"("randomized_trigger_rate":0.0024,)"
       R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
       R"("source_event_id":"100",)"
       R"("source_type":"navigation",)"
       R"("trigger_data":"5"})"},
      {CommonSourceInfo::SourceType::kEvent,
       R"({"attribution_destination":"https://conversion.test",)"
       R"("randomized_trigger_rate":0.0000025,)"
       R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
       R"("source_event_id":"100",)"
       R"("source_type":"event",)"
       R"("trigger_data":"5"})"},
  };

  for (const auto& test_case : kTestCases) {
    auto impression = SourceBuilder(base::Time())
                          .SetSourceEventId(100)
                          .SetSourceType(test_case.source_type)
                          .BuildStored();
    AttributionReport report =
        ReportBuilder(AttributionInfoBuilder(impression).Build())
            .SetTriggerData(5)
            .Build();
    network_sender_->SendReport(report, base::DoNothing());

    const network::ResourceRequest* pending_request;
    EXPECT_TRUE(
        test_url_loader_factory_.IsPending(kReportUrl, &pending_request));
    EXPECT_EQ(test_case.expected_report,
              network::GetUploadData(*pending_request));
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kReportUrl, ""));
  }
}

TEST_F(AttributionNetworkSenderTest,
       ReportSentWithDebugKeys_ReportBodySetCorrectly) {
  const struct {
    absl::optional<uint64_t> source_debug_key;
    absl::optional<uint64_t> trigger_debug_key;
    const char* expected_report;
  } kTestCases[] = {
      {absl::nullopt, absl::nullopt,
       R"({"attribution_destination":"https://conversion.test",)"
       R"("randomized_trigger_rate":0.0024,)"
       R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
       R"("source_event_id":"100",)"
       R"("source_type":"navigation",)"
       R"("trigger_data":"5"})"},
      {7, absl::nullopt,
       R"({"attribution_destination":"https://conversion.test",)"
       R"("randomized_trigger_rate":0.0024,)"
       R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
       R"("source_debug_key":"7",)"
       R"("source_event_id":"100",)"
       R"("source_type":"navigation",)"
       R"("trigger_data":"5"})"},
      {absl::nullopt, 7,
       R"({"attribution_destination":"https://conversion.test",)"
       R"("randomized_trigger_rate":0.0024,)"
       R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
       R"("source_event_id":"100",)"
       R"("source_type":"navigation",)"
       R"("trigger_data":"5",)"
       R"("trigger_debug_key":"7"})"},
      {7, 8,
       R"({"attribution_destination":"https://conversion.test",)"
       R"("randomized_trigger_rate":0.0024,)"
       R"("report_id":"21abd97f-73e8-4b88-9389-a9fee6abda5e",)"
       R"("source_debug_key":"7",)"
       R"("source_event_id":"100",)"
       R"("source_type":"navigation",)"
       R"("trigger_data":"5",)"
       R"("trigger_debug_key":"8"})"},
  };

  for (const auto& test_case : kTestCases) {
    auto impression = SourceBuilder(base::Time())
                          .SetSourceEventId(100)
                          .SetDebugKey(test_case.source_debug_key)
                          .BuildStored();
    AttributionReport report =
        ReportBuilder(AttributionInfoBuilder(impression)
                          .SetDebugKey(test_case.trigger_debug_key)
                          .Build())
            .SetTriggerData(5)
            .Build();
    network_sender_->SendReport(report, base::DoNothing());

    const network::ResourceRequest* pending_request;
    EXPECT_TRUE(
        test_url_loader_factory_.IsPending(kReportUrl, &pending_request));
    EXPECT_EQ(test_case.expected_report,
              network::GetUploadData(*pending_request));
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kReportUrl, ""));
  }
}

TEST_F(AttributionNetworkSenderTest, ReportSent_RequestAttributesSet) {
  auto impression =
      SourceBuilder(base::Time())
          .SetReportingOrigin(url::Origin::Create(GURL("https://a.com")))
          .SetConversionOrigin(url::Origin::Create(GURL("https://sub.b.com")))
          .BuildStored();
  AttributionReport report =
      ReportBuilder(AttributionInfoBuilder(impression).Build()).Build();
  network_sender_->SendReport(report, base::DoNothing());

  const network::ResourceRequest* pending_request;
  EXPECT_TRUE(test_url_loader_factory_.IsPending(
      "https://a.com/.well-known/attribution-reporting/report-attribution",
      &pending_request));

  // Ensure that the request is sent with no credentials.
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            pending_request->credentials_mode);
  EXPECT_EQ("POST", pending_request->method);

  EXPECT_EQ(GURL(), pending_request->referrer);
}

TEST_F(AttributionNetworkSenderTest, ReportSent_CallbackFired) {
  auto report = DefaultReport();
  EXPECT_CALL(callback_, Run(report, SendResult(SendResult::Status::kSent,
                                                net::HttpStatusCode::HTTP_OK)));

  network_sender_->SendReport(report, callback_.Get());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, ""));
}

TEST_F(AttributionNetworkSenderTest, SenderDeletedDuringRequest_NoCrash) {
  EXPECT_CALL(callback_, Run).Times(0);

  auto report = DefaultReport();
  network_sender_->SendReport(report, callback_.Get());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  network_sender_.reset();
  EXPECT_FALSE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, ""));
}

TEST_F(AttributionNetworkSenderTest, ReportRequestHangs_TimesOut) {
  auto report = DefaultReport();

  // Verify that the sent callback runs if the request times out.
  // TODO(apaseltiner): Should we propagate the timeout via the SendResult
  // instead of just setting |http_response_code = 0|?
  EXPECT_CALL(callback_,
              Run(report, SendResult(SendResult::Status::kTransientFailure,
                                     /*http_response_code=*/0)));
  network_sender_->SendReport(report, callback_.Get());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  // The request should time out after 30 seconds.
  task_environment_.FastForwardBy(base::Seconds(30));

  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(AttributionNetworkSenderTest,
       ReportRequestFailsWithTargetedError_ShouldRetrySet) {
  struct {
    int net_error;
    SendResult::Status expected_status;
  } kTestCases[] = {
      {net::ERR_INTERNET_DISCONNECTED, SendResult::Status::kTransientFailure},
      {net::ERR_TIMED_OUT, SendResult::Status::kTransientFailure},
      {net::ERR_CONNECTION_ABORTED, SendResult::Status::kTransientFailure},
      {net::ERR_CONNECTION_TIMED_OUT, SendResult::Status::kTransientFailure},
      {net::ERR_CONNECTION_REFUSED, SendResult::Status::kFailure},
      {net::ERR_CERT_DATE_INVALID, SendResult::Status::kFailure},
      {net::OK, SendResult::Status::kFailure},
  };

  for (const auto& test_case : kTestCases) {
    auto report = DefaultReport();

    EXPECT_CALL(callback_, Run(report, Field(&SendResult::status,
                                             test_case.expected_status)));

    network_sender_->SendReport(report, callback_.Get());
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // By default, headers are not sent for network errors.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kReportUrl),
        network::URLLoaderCompletionStatus(test_case.net_error),
        network::mojom::URLResponseHead::New(), "");

    Mock::VerifyAndClear(&callback_);
  }
}

TEST_F(AttributionNetworkSenderTest, ReportRequestFailsWithHeaders_NotRetried) {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");

  // Simulate a retry-able network error with headers received.
  test_url_loader_factory_.AddResponse(
      GURL(kReportUrl),
      /*head=*/std::move(head), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED),
      network::TestURLLoaderFactory::Redirects(),
      network::TestURLLoaderFactory::ResponseProduceFlags::
          kSendHeadersOnNetworkError);

  auto report = DefaultReport();
  EXPECT_CALL(callback_, Run(report, SendResult(SendResult::Status::kFailure,
                                                net::HttpStatusCode::HTTP_OK)));
  network_sender_->SendReport(report, callback_.Get());

  // Ensure the request was replied to.
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(AttributionNetworkSenderTest,
       ReportRequestFailsWithHttpError_ShouldRetryNotSet) {
  auto report = DefaultReport();
  EXPECT_CALL(callback_,
              Run(report, SendResult(SendResult::Status::kFailure,
                                     net::HttpStatusCode::HTTP_BAD_REQUEST)));

  network_sender_->SendReport(report, callback_.Get());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());

  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, "", net::HttpStatusCode::HTTP_BAD_REQUEST));
}

TEST_F(AttributionNetworkSenderTest,
       ReportRequestFailsDueToNetworkChange_Retries) {
  // Retry fails
  {
    base::HistogramTester histograms;

    EXPECT_CALL(callback_, Run);

    auto report = DefaultReport();
    network_sender_->SendReport(report, callback_.Get());
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), "");

    // The sender should automatically retry.
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate a second request failure due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), "");

    // We should not retry again. Verify the report sent callback only gets
    // fired once.
    EXPECT_EQ(0, test_url_loader_factory_.NumPending());
    Mock::VerifyAndClear(&callback_);

    histograms.ExpectUniqueSample("Conversions.ReportRetrySucceed", false, 1);
  }

  // Retry succeeds
  {
    base::HistogramTester histograms;

    auto report = DefaultReport();
    network_sender_->SendReport(report, base::DoNothing());
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate the request failing due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kReportUrl),
        network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED),
        network::mojom::URLResponseHead::New(), "");

    // The sender should automatically retry.
    EXPECT_EQ(1, test_url_loader_factory_.NumPending());

    // Simulate a second request failure due to network change.
    test_url_loader_factory_.SimulateResponseForPendingRequest(kReportUrl, "");

    histograms.ExpectUniqueSample("Conversions.ReportRetrySucceed", true, 1);
  }
}

TEST_F(AttributionNetworkSenderTest,
       ReportResultsInHttpError_SentCallbackRuns) {
  auto report = DefaultReport();

  Checkpoint checkpoint;
  {
    InSequence seq;

    EXPECT_CALL(callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(callback_,
                Run(report, SendResult(SendResult::Status::kFailure,
                                       net::HttpStatusCode::HTTP_BAD_REQUEST)));
  }

  network_sender_->SendReport(report, callback_.Get());
  checkpoint.Call(1);

  // We should run the sent callback even if there is an http error.
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      kReportUrl, "", net::HttpStatusCode::HTTP_BAD_REQUEST));
}

TEST_F(AttributionNetworkSenderTest, ManyReports_AllSentSuccessfully) {
  EXPECT_CALL(callback_, Run).Times(10);

  for (int i = 0; i < 10; i++) {
    auto report = DefaultReport();
    network_sender_->SendReport(report, callback_.Get());
  }
  EXPECT_EQ(10, test_url_loader_factory_.NumPending());

  // Send reports out of order to guarantee that callback conversion_ids are
  // properly handled.
  for (int i = 9; i >= 0; i--) {
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kReportUrl, ""));
  }
  EXPECT_EQ(0, test_url_loader_factory_.NumPending());
}

TEST_F(AttributionNetworkSenderTest, ErrorHistogram) {
  // All OK.
  {
    base::HistogramTester histograms;
    auto report = DefaultReport();
    network_sender_->SendReport(report, base::DoNothing());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kReportUrl, ""));
    // kOk = 0.
    histograms.ExpectUniqueSample("Conversions.ReportStatus", 0, 1);
    histograms.ExpectUniqueSample(
        "Conversions.Report.HttpResponseOrNetErrorCode", net::HTTP_OK, 1);
  }
  // Internal error.
  {
    base::HistogramTester histograms;
    auto report = DefaultReport();
    network_sender_->SendReport(report, base::DoNothing());
    network::URLLoaderCompletionStatus completion_status(net::ERR_FAILED);
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        GURL(kReportUrl), completion_status,
        network::mojom::URLResponseHead::New(), ""));
    // kInternalError = 1.
    histograms.ExpectUniqueSample("Conversions.ReportStatus", 1, 1);
    histograms.ExpectUniqueSample(
        "Conversions.Report.HttpResponseOrNetErrorCode", net::ERR_FAILED, 1);
  }
  {
    base::HistogramTester histograms;
    auto report = DefaultReport();
    network_sender_->SendReport(report, base::DoNothing());
    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        kReportUrl, "", net::HTTP_UNAUTHORIZED));
    // kExternalError = 2.
    histograms.ExpectUniqueSample("Conversions.ReportStatus", 2, 1);
    histograms.ExpectUniqueSample(
        "Conversions.Report.HttpResponseOrNetErrorCode", net::HTTP_UNAUTHORIZED,
        1);
  }
}

}  // namespace content
