// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_loader_status.h"

#include "net/base/net_errors.h"

namespace network {

URLLoaderStatus::URLLoaderStatus() = default;
URLLoaderStatus::URLLoaderStatus(const URLLoaderStatus& status) = default;

URLLoaderStatus::URLLoaderStatus(int error_code)
    : error_code(error_code), completion_time(base::TimeTicks::Now()) {}

URLLoaderStatus::URLLoaderStatus(network::mojom::CORSError error)
    : URLLoaderStatus(net::ERR_FAILED) {
  cors_error = error;
}

URLLoaderStatus::~URLLoaderStatus() = default;

bool URLLoaderStatus::operator==(const URLLoaderStatus& rhs) const {
  return error_code == rhs.error_code &&
         exists_in_cache == rhs.exists_in_cache &&
         completion_time == rhs.completion_time &&
         encoded_data_length == rhs.encoded_data_length &&
         encoded_body_length == rhs.encoded_body_length &&
         decoded_body_length == rhs.decoded_body_length &&
         cors_error == rhs.cors_error;
}

}  // namespace network
