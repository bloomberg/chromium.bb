// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_NETWORK_REQUEST_ERROR_H_
#define ASH_SERVICES_DEVICE_SYNC_NETWORK_REQUEST_ERROR_H_

#include <ostream>

namespace ash {

namespace device_sync {

enum class NetworkRequestError {
  // Request could not be completed because the device is offline or has issues
  // sending the HTTP request.
  kOffline,

  // Server endpoint could not be found.
  kEndpointNotFound,

  // Authentication error contacting back-end.
  kAuthenticationError,

  // Request was invalid.
  kBadRequest,

  // The server responded, but the response was not formatted correctly.
  kResponseMalformed,

  // Internal server error.
  kInternalServerError,

  // Unknown result.
  kUnknown
};

std::ostream& operator<<(std::ostream& stream,
                         const NetworkRequestError& error);

}  // namespace device_sync

}  // namespace ash

#endif  // ASH_SERVICES_DEVICE_SYNC_NETWORK_REQUEST_ERROR_H_
