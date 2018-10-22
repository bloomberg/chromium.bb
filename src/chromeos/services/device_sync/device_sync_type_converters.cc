// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/device_sync_type_converters.h"

namespace mojo {

// static
chromeos::device_sync::mojom::NetworkRequestResult TypeConverter<
    chromeos::device_sync::mojom::NetworkRequestResult,
    cryptauth::NetworkRequestError>::Convert(cryptauth::NetworkRequestError
                                                 type) {
  switch (type) {
    case cryptauth::NetworkRequestError::kOffline:
      return chromeos::device_sync::mojom::NetworkRequestResult::kOffline;
    case cryptauth::NetworkRequestError::kEndpointNotFound:
      return chromeos::device_sync::mojom::NetworkRequestResult::
          kEndpointNotFound;
    case cryptauth::NetworkRequestError::kAuthenticationError:
      return chromeos::device_sync::mojom::NetworkRequestResult::
          kAuthenticationError;
    case cryptauth::NetworkRequestError::kBadRequest:
      return chromeos::device_sync::mojom::NetworkRequestResult::kBadRequest;
    case cryptauth::NetworkRequestError::kResponseMalformed:
      return chromeos::device_sync::mojom::NetworkRequestResult::
          kResponseMalformed;
    case cryptauth::NetworkRequestError::kInternalServerError:
      return chromeos::device_sync::mojom::NetworkRequestResult::
          kInternalServerError;
    case cryptauth::NetworkRequestError::kUnknown:
      return chromeos::device_sync::mojom::NetworkRequestResult::kUnknown;
  }
}

}  // namespace mojo
