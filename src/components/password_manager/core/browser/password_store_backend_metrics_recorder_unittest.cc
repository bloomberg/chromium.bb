// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend_metrics_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_backend_metrics_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {
using testing::ElementsAre;

using SuccessStatus = PasswordStoreBackendMetricsRecorder::SuccessStatus;

constexpr auto kLatencyDelta = base::Milliseconds(123u);

constexpr char kSomeBackend[] = "SomeBackend";
constexpr char kSomeMethod[] = "MethodName";
constexpr char kDurationMetric[] =
    "PasswordManager.PasswordStoreSomeBackend.MethodName.Latency";
constexpr char kSuccessMetric[] =
    "PasswordManager.PasswordStoreSomeBackend.MethodName.Success";
constexpr char kErrorCodeMetric[] =
    "PasswordManager.PasswordStoreSomeBackend.MethodName.ErrorCode";
constexpr char kApiErrorMetric[] =
    "PasswordManager.PasswordStoreSomeBackend.MethodName.APIError";
constexpr char kDurationOverallMetric[] =
    "PasswordManager.PasswordStoreBackend.MethodName.Latency";
constexpr char kSuccessOverallMetric[] =
    "PasswordManager.PasswordStoreBackend.MethodName.Success";
constexpr char kErrorCodeOverallMetric[] =
    "PasswordManager.PasswordStoreAndroidBackend.ErrorCode";
constexpr char kApiErrorOverallMetric[] =
    "PasswordManager.PasswordStoreAndroidBackend.APIError";
}  // anonymous namespace

class PasswordStoreBackendMetricsRecorderTest : public testing::Test {
 public:
  void AdvanceClock(base::TimeDelta millis) {
    // AdvanceClock is used here because FastForwardBy doesn't work for the
    // intended purpose. FastForwardBy performs the queued actions first and
    // then makes the clock tick and for the tests that follow we want to
    // advance the clock before certain async tasks happen.
    task_environment_.AdvanceClock(millis);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PasswordStoreBackendMetricsRecorderTest, RecordMetrics_Success) {
  using base::Bucket;
  base::HistogramTester histogram_tester;

  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(BackendInfix(kSomeBackend),
                                          MetricInfix(kSomeMethod));

  AdvanceClock(kLatencyDelta);

  metrics_recorder.RecordMetrics(SuccessStatus::kSuccess,
                                 /*error=*/absl::nullopt);

  // Checking records in the backend-specific histogram
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetric),
              ElementsAre(Bucket(true, 1)));

  // Checking records in the overall histogram
  histogram_tester.ExpectTotalCount(kDurationOverallMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationOverallMetric, kLatencyDelta,
                                         1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessOverallMetric),
              ElementsAre(Bucket(true, 1)));
}

TEST_F(PasswordStoreBackendMetricsRecorderTest, RecordMetrics_ExternalError) {
  using base::Bucket;
  base::HistogramTester histogram_tester;

  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(BackendInfix(kSomeBackend),
                                          MetricInfix(kSomeMethod));

  AdvanceClock(kLatencyDelta);

  AndroidBackendError error(AndroidBackendErrorType::kExternalError);
  error.api_error_code = 11010;
  metrics_recorder.RecordMetrics(SuccessStatus::kError, std::move(error));

  // Checking records in the backend-specific histogram
  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetric),
              ElementsAre(Bucket(false, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeMetric),
              ElementsAre(Bucket(7, 1)));  // External
  EXPECT_THAT(histogram_tester.GetAllSamples(kApiErrorMetric),
              ElementsAre(Bucket(11010, 1)));  // No access.

  // Checking records in the overall histogram
  histogram_tester.ExpectTotalCount(kDurationOverallMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationOverallMetric, kLatencyDelta,
                                         1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessOverallMetric),
              ElementsAre(Bucket(false, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kErrorCodeOverallMetric),
              ElementsAre(Bucket(7, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(kApiErrorOverallMetric),
              ElementsAre(Bucket(11010, 1)));
}

TEST_F(PasswordStoreBackendMetricsRecorderTest, RecordMetrics_Cancelled) {
  using base::Bucket;
  base::HistogramTester histogram_tester;

  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(BackendInfix(kSomeBackend),
                                          MetricInfix(kSomeMethod));

  AdvanceClock(kLatencyDelta);

  metrics_recorder.RecordMetrics(SuccessStatus::kCancelled,
                                 /*error=*/absl::nullopt);

  // Checking records in the backend-specific histogram
  histogram_tester.ExpectTotalCount(kDurationMetric, 0);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessMetric),
              ElementsAre(Bucket(false, 1)));

  // Checking records in the overall histogram
  histogram_tester.ExpectTotalCount(kDurationOverallMetric, 0);
  EXPECT_THAT(histogram_tester.GetAllSamples(kSuccessOverallMetric),
              ElementsAre(Bucket(false, 1)));
}

}  // namespace password_manager
