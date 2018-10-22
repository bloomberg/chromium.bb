// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gcm_unregistration_request_handler.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "google_apis/gcm/base/gcm_util.h"
#include "net/url_request/url_fetcher.h"

namespace gcm {

namespace {

// Request constants.
const char kUnregistrationCallerKey[] = "gcm_unreg_caller";
// We are going to set the value to "false" in order to forcefully unregister
// the application.
const char kUnregistrationCallerValue[] = "false";

// Response constants.
const char kDeletedPrefix[] = "deleted=";

}  // namespace

GCMUnregistrationRequestHandler::GCMUnregistrationRequestHandler(
    const std::string& app_id)
    : app_id_(app_id) {
}

GCMUnregistrationRequestHandler::~GCMUnregistrationRequestHandler() {}

void GCMUnregistrationRequestHandler::BuildRequestBody(std::string* body){
  BuildFormEncoding(kUnregistrationCallerKey, kUnregistrationCallerValue, body);
}

UnregistrationRequest::Status GCMUnregistrationRequestHandler::ParseResponse(
    const std::string& response) {
  DVLOG(1) << "Parsing unregistration response.";
  if (response.find(kDeletedPrefix) != std::string::npos) {
    std::string deleted_app_id = response.substr(
        response.find(kDeletedPrefix) + arraysize(kDeletedPrefix) - 1);
    return deleted_app_id == app_id_ ?
        UnregistrationRequest::SUCCESS :
        UnregistrationRequest::INCORRECT_APP_ID;
  }

  DVLOG(1) << "Not able to parse a meaningful output from response body."
           << response;
  return UnregistrationRequest::RESPONSE_PARSING_FAILED;
}

void GCMUnregistrationRequestHandler::ReportUMAs(
    UnregistrationRequest::Status status,
    int retry_count,
    base::TimeDelta complete_time) {
  UMA_HISTOGRAM_ENUMERATION("GCM.UnregistrationRequestStatus",
                            status,
                            UnregistrationRequest::UNREGISTRATION_STATUS_COUNT);

  // Other UMAs are only reported when the request succeeds.
  if (status != UnregistrationRequest::SUCCESS)
    return;

  UMA_HISTOGRAM_COUNTS("GCM.UnregistrationRetryCount", retry_count);
  UMA_HISTOGRAM_TIMES("GCM.UnregistrationCompleteTime", complete_time);
}

}  // namespace gcm
