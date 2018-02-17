// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/network_error_logging_delegate.h"

namespace net {

NetworkErrorLoggingDelegate::RequestDetails::RequestDetails() = default;

NetworkErrorLoggingDelegate::RequestDetails::RequestDetails(
    const RequestDetails& other) = default;

NetworkErrorLoggingDelegate::RequestDetails::~RequestDetails() = default;

bool NetworkErrorLoggingDelegate::RequestDetails::IsHttpError() const {
  return status_code >= 400 && status_code < 600;
}

bool NetworkErrorLoggingDelegate::RequestDetails::RequestWasSuccessful() const {
  return type == OK && !IsHttpError();
}

const char NetworkErrorLoggingDelegate::kHeaderName[] = "NEL";

NetworkErrorLoggingDelegate::~NetworkErrorLoggingDelegate() = default;

}  // namespace net
