// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_operation_metrics_recorder.h"

#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(TrustTokenOperationMetricsRecorder, Success) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TrustTokenOperationMetricsRecorder recorder;
  base::HistogramTester histograms;

  recorder.BeginBegin(mojom::TrustTokenOperationType::kIssuance);
  env.FastForwardBy(base::TimeDelta::FromSeconds(1));
  recorder.FinishBegin(mojom::TrustTokenOperationStatus::kOk);

  env.FastForwardBy(base::TimeDelta::FromSeconds(2));
  recorder.BeginFinalize();
  env.FastForwardBy(base::TimeDelta::FromSeconds(3));
  recorder.FinishFinalize(mojom::TrustTokenOperationStatus::kOk);

  histograms.ExpectUniqueTimeSample(
      base::JoinString({internal::kTrustTokenBeginTimeHistogramNameBase,
                        "Success", "Issuance"},
                       "."),
      base::TimeDelta::FromSeconds(1),
      /*expected_count=*/1);

  histograms.ExpectUniqueTimeSample(
      base::JoinString({internal::kTrustTokenServerTimeHistogramNameBase,
                        "Success", "Issuance"},
                       "."),
      base::TimeDelta::FromSeconds(2),
      /*expected_count=*/1);

  histograms.ExpectUniqueTimeSample(
      base::JoinString({internal::kTrustTokenFinalizeTimeHistogramNameBase,
                        "Success", "Issuance"},
                       "."),
      base::TimeDelta::FromSeconds(3),
      /*expected_count=*/1);

  histograms.ExpectUniqueTimeSample(
      base::JoinString({internal::kTrustTokenTotalTimeHistogramNameBase,
                        "Success", "Issuance"},
                       "."),
      base::TimeDelta::FromSeconds(1 + 2 + 3),
      /*expected_count=*/1);
}

TEST(TrustTokenOperationMetricsRecorder, BeginFailure) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TrustTokenOperationMetricsRecorder recorder;
  base::HistogramTester histograms;

  recorder.BeginBegin(mojom::TrustTokenOperationType::kRedemption);
  env.FastForwardBy(base::TimeDelta::FromSeconds(1));
  recorder.FinishBegin(mojom::TrustTokenOperationStatus::kUnknownError);

  histograms.ExpectUniqueTimeSample(
      base::JoinString({internal::kTrustTokenBeginTimeHistogramNameBase,
                        "Failure", "Redemption"},
                       "."),
      base::TimeDelta::FromSeconds(1),
      /*expected_count=*/1);
}

TEST(TrustTokenOperationMetricsRecorder, FinalizeFailure) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TrustTokenOperationMetricsRecorder recorder;
  base::HistogramTester histograms;

  recorder.BeginBegin(mojom::TrustTokenOperationType::kSigning);
  env.FastForwardBy(base::TimeDelta::FromSeconds(1));
  recorder.FinishBegin(mojom::TrustTokenOperationStatus::kOk);

  env.FastForwardBy(base::TimeDelta::FromSeconds(2));
  recorder.BeginFinalize();
  env.FastForwardBy(base::TimeDelta::FromSeconds(3));
  recorder.FinishFinalize(mojom::TrustTokenOperationStatus::kUnknownError);

  histograms.ExpectUniqueTimeSample(
      base::JoinString({internal::kTrustTokenBeginTimeHistogramNameBase,
                        "Success", "Signing"},
                       "."),
      base::TimeDelta::FromSeconds(1),
      /*expected_count=*/1);

  histograms.ExpectUniqueTimeSample(
      base::JoinString({internal::kTrustTokenServerTimeHistogramNameBase,
                        "Failure", "Signing"},
                       "."),
      base::TimeDelta::FromSeconds(2),
      /*expected_count=*/1);

  histograms.ExpectUniqueTimeSample(
      base::JoinString({internal::kTrustTokenFinalizeTimeHistogramNameBase,
                        "Failure", "Signing"},
                       "."),
      base::TimeDelta::FromSeconds(3),
      /*expected_count=*/1);

  histograms.ExpectUniqueTimeSample(
      base::JoinString({internal::kTrustTokenTotalTimeHistogramNameBase,
                        "Failure", "Signing"},
                       "."),
      base::TimeDelta::FromSeconds(1 + 2 + 3),
      /*expected_count=*/1);
}

}  // namespace network
