// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend_metrics_recorder.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"

namespace password_manager {

namespace {
constexpr char kMetricPrefix[] = "PasswordManager.PasswordStore";

bool ShouldRecordLatency(
    PasswordStoreBackendMetricsRecorder::SuccessStatus success_status) {
  switch (success_status) {
    case PasswordStoreBackendMetricsRecorder::SuccessStatus::kSuccess:
    case PasswordStoreBackendMetricsRecorder::SuccessStatus::kError:
      return true;
    case PasswordStoreBackendMetricsRecorder::SuccessStatus::kCancelled:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder() =
    default;

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder(
    BackendInfix backend_infix,
    MetricInfix metric_infix)
    : backend_infix_(std::move(backend_infix)),
      metric_infix_(std::move(metric_infix)) {}

PasswordStoreBackendMetricsRecorder::PasswordStoreBackendMetricsRecorder(
    PasswordStoreBackendMetricsRecorder&&) = default;

PasswordStoreBackendMetricsRecorder& PasswordStoreBackendMetricsRecorder::
    PasswordStoreBackendMetricsRecorder::operator=(
        PasswordStoreBackendMetricsRecorder&&) = default;

PasswordStoreBackendMetricsRecorder::~PasswordStoreBackendMetricsRecorder() =
    default;

void PasswordStoreBackendMetricsRecorder::RecordMetrics(
    SuccessStatus success_status,
    absl::optional<ErrorFromPasswordStoreOrAndroidBackend> error) const {
  RecordSuccess(success_status);
  if (ShouldRecordLatency(success_status)) {
    RecordLatency();
  }
  if (error.has_value()) {
    DCHECK_NE(success_status, SuccessStatus::kSuccess);
    if (absl::holds_alternative<AndroidBackendError>(error.value())) {
      RecordErrorCode(std::move(absl::get<1>(error.value())));
    }
  }
}

void PasswordStoreBackendMetricsRecorder::RecordMetricsForUnenrolledClients(
    const absl::optional<AndroidBackendError>& error) const {
  base::UmaHistogramBoolean(BuildMetricName("UnenrolledFromUPM.Success"),
                            !error.has_value());
  if (!error.has_value())
    return;

  base::UmaHistogramEnumeration(BuildMetricName("UnenrolledFromUPM.ErrorCode"),
                                error->type);
  if (error->type == AndroidBackendErrorType::kExternalError) {
    DCHECK(error->api_error_code.has_value());
    base::UmaHistogramSparse(BuildMetricName("UnenrolledFromUPM.APIError"),
                             error->api_error_code.value());
  }
}

base::TimeDelta
PasswordStoreBackendMetricsRecorder::GetElapsedTimeSinceCreation() const {
  return base::Time::Now() - start_;
}

void PasswordStoreBackendMetricsRecorder::RecordSuccess(
    SuccessStatus success_status) const {
  base::UmaHistogramBoolean(BuildMetricName("Success"),
                            success_status == SuccessStatus::kSuccess);
  base::UmaHistogramBoolean(BuildOverallMetricName("Success"),
                            success_status == SuccessStatus::kSuccess);
}

void PasswordStoreBackendMetricsRecorder::RecordErrorCode(
    const AndroidBackendError& backend_error) const {
  base::UmaHistogramEnumeration(
      base::StrCat({kMetricPrefix, "AndroidBackend.ErrorCode"}),
      backend_error.type);
  base::UmaHistogramEnumeration(BuildMetricName("ErrorCode"),
                                backend_error.type);
  if (backend_error.type == AndroidBackendErrorType::kExternalError) {
    DCHECK(backend_error.api_error_code.has_value());
    RecordApiErrorCode(backend_error.api_error_code.value());
    LOG(ERROR) << "Password Manager API call for " << metric_infix_
               << " failed with error code: "
               << backend_error.api_error_code.value();
  }
}

void PasswordStoreBackendMetricsRecorder::RecordLatency() const {
  base::TimeDelta duration = GetElapsedTimeSinceCreation();
  base::UmaHistogramMediumTimes(BuildMetricName("Latency"), duration);
  base::UmaHistogramMediumTimes(BuildOverallMetricName("Latency"), duration);
}

void PasswordStoreBackendMetricsRecorder::RecordApiErrorCode(
    int api_error_code) const {
  base::UmaHistogramSparse(
      base::StrCat({kMetricPrefix, "AndroidBackend.APIError"}), api_error_code);
  base::UmaHistogramSparse(BuildMetricName("APIError"), api_error_code);
}

std::string PasswordStoreBackendMetricsRecorder::BuildMetricName(
    base::StringPiece suffix) const {
  return base::StrCat(
      {kMetricPrefix, *backend_infix_, ".", *metric_infix_, ".", suffix});
}

std::string PasswordStoreBackendMetricsRecorder::BuildOverallMetricName(
    base::StringPiece suffix) const {
  return base::StrCat({kMetricPrefix, "Backend.", *metric_infix_, ".", suffix});
}
}  // namespace password_manager
