// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors_error_status.h"

#include "net/base/net_errors.h"

namespace network {

// Note: |cors_error| is initialized to kLast to keep the value inside the
// valid enum value range. The value is meaningless and should be overriden
// immediately by IPC desrtialization code.
CORSErrorStatus::CORSErrorStatus()
    : CORSErrorStatus(network::mojom::CORSError::kMaxValue) {}

CORSErrorStatus::CORSErrorStatus(const CORSErrorStatus& status) = default;

CORSErrorStatus::CORSErrorStatus(network::mojom::CORSError error)
    : cors_error(error) {}

CORSErrorStatus::CORSErrorStatus(
    network::mojom::CORSError error,
    scoped_refptr<net::HttpResponseHeaders> headers)
    : CORSErrorStatus(error) {
  related_response_headers = headers;
}

CORSErrorStatus::~CORSErrorStatus() = default;

bool CORSErrorStatus::operator==(const CORSErrorStatus& rhs) const {
  return cors_error == rhs.cors_error &&
         related_response_headers == rhs.related_response_headers;
}

}  // namespace network
