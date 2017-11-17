// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_STATUS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_STATUS_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "services/network/public/interfaces/cors.mojom.h"

namespace network {

struct URLLoaderStatus {
  URLLoaderStatus();
  URLLoaderStatus(const URLLoaderStatus& status);

  // Sets |error_code| to |error_code| and base::TimeTicks::Now() to
  // |completion_time|.
  explicit URLLoaderStatus(int error_code);

  // Sets ERR_FAILED to |error_code|, |error| to |cors_error|, and
  // base::TimeTicks::Now() to |completion_time|.
  explicit URLLoaderStatus(network::mojom::CORSError error);

  ~URLLoaderStatus();

  bool operator==(const URLLoaderStatus& rhs) const;

  // The error code. ERR_FAILED is set for CORS errors.
  int error_code = 0;

  // A copy of the data requested exists in the cache.
  bool exists_in_cache = false;

  // Time the request completed.
  base::TimeTicks completion_time;

  // Total amount of data received from the network.
  int64_t encoded_data_length = 0;

  // The length of the response body before removing any content encodings.
  int64_t encoded_body_length = 0;

  // The length of the response body after decoding.
  int64_t decoded_body_length = 0;

  // Optional CORS error details.
  base::Optional<network::mojom::CORSError> cors_error;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_LOADER_STATUS_H_
