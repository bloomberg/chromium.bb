// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_loader_completion_status.h"

#include "net/base/net_errors.h"

namespace network {

URLLoaderCompletionStatus::URLLoaderCompletionStatus() = default;
URLLoaderCompletionStatus::URLLoaderCompletionStatus(
    const URLLoaderCompletionStatus& status) = default;

URLLoaderCompletionStatus::URLLoaderCompletionStatus(int error_code)
    : error_code(error_code), completion_time(base::TimeTicks::Now()) {}

URLLoaderCompletionStatus::URLLoaderCompletionStatus(
    const CORSErrorStatus& error)
    : URLLoaderCompletionStatus(net::ERR_FAILED) {
  cors_error_status = error;
}

URLLoaderCompletionStatus::~URLLoaderCompletionStatus() = default;

bool URLLoaderCompletionStatus::operator==(
    const URLLoaderCompletionStatus& rhs) const {
  return error_code == rhs.error_code &&
         exists_in_cache == rhs.exists_in_cache &&
         completion_time == rhs.completion_time &&
         encoded_data_length == rhs.encoded_data_length &&
         encoded_body_length == rhs.encoded_body_length &&
         decoded_body_length == rhs.decoded_body_length &&
         cors_error_status == rhs.cors_error_status &&
         blocked_cross_site_document == rhs.blocked_cross_site_document;
}

}  // namespace network
