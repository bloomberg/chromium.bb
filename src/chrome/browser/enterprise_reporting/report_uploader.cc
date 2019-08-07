// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/report_uploader.h"

#include <utility>

#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {
// Retry starts with 1 minute delay and is doubled with every failure.
const net::BackoffEntry::Policy kDefaultReportUploadBackoffPolicy = {
    0,      // Number of initial errors to ignore before applying
            // exponential back-off rules.
    60000,  // Initial delay is 60 seconds.
    2,      // Factor by which the waiting time will be multiplied.
    0.1,    // Fuzzing percentage.
    -1,     // No maximum delay.
    -1,     // It's up to the caller to reset the backoff time.
    false   // Do not always use initial delay.
};

}  // namespace

ReportUploader::ReportUploader(policy::CloudPolicyClient* client,
                               int maximum_number_of_retries)
    : client_(client),
      backoff_entry_(&kDefaultReportUploadBackoffPolicy),
      maximum_number_of_retries_(maximum_number_of_retries),
      weak_ptr_factory_(this) {}
ReportUploader::~ReportUploader() = default;

void ReportUploader::SetRequestAndUpload(
    std::queue<std::unique_ptr<em::ChromeDesktopReportRequest>> requests,
    ReportCallback callback) {
  requests_ = std::move(requests);
  callback_ = std::move(callback);
  Upload();
}

void ReportUploader::Upload() {
  client_->UploadChromeDesktopReport(
      std::make_unique<em::ChromeDesktopReportRequest>(*requests_.front()),
      base::BindRepeating(&ReportUploader::OnRequestFinished,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ReportUploader::OnRequestFinished(bool status) {
  if (status) {
    // We don't reset the backoff in case there are multiple requests in a row
    // and we don't start from 1 minute again.
    backoff_entry_.InformOfRequest(true);
    requests_.pop();
    if (requests_.size() == 0)
      SendResponse(ReportStatus::kSuccess);
    else
      Upload();
    return;
  }

  switch (client_->status()) {
    case policy::DM_STATUS_REQUEST_FAILED:         // network error
    case policy::DM_STATUS_TEMPORARY_UNAVAILABLE:  // 5xx server error
    // DM_STATUS_SERVICE_DEVICE_ID_CONFLICT is caused by 409 conflict. It can
    // be caused by either device id conflict or DDS concur error which is
    // a database error. We only want to retry for the second case. However,
    // there is no way for us to tell difference right now so we will retry
    // regardless.
    case policy::DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
      Retry();
      break;
    default:
      // TODO(zmin): Handle error 413.
      SendResponse(ReportStatus::kPersistentError);
      break;
  }
}

void ReportUploader::Retry() {
  backoff_entry_.InformOfRequest(false);
  // We have retried enough, time to give up.
  if (HasRetriedTooOften()) {
    SendResponse(ReportStatus::kTransientError);
    return;
  }
  backoff_request_timer_.Start(
      FROM_HERE, backoff_entry_.GetTimeUntilRelease(),
      base::BindOnce(&ReportUploader::Upload, weak_ptr_factory_.GetWeakPtr()));
}

bool ReportUploader::HasRetriedTooOften() {
  return backoff_entry_.failure_count() > maximum_number_of_retries_;
}

void ReportUploader::SendResponse(const ReportStatus status) {
  std::move(callback_).Run(status);
}

}  // namespace enterprise_reporting
