// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_REQUEST_DESTINATION_H_
#define SERVICES_NETWORK_PUBLIC_CPP_REQUEST_DESTINATION_H_

#include "base/component_export.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace network {

// Returns a string corresponding to the |destination| as defined in the spec:
// https://fetch.spec.whatwg.org/#concept-request-destination.
COMPONENT_EXPORT(NETWORK_CPP)
const char* RequestDestinationToString(network::mojom::RequestDestination dest);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_REQUEST_DESTINATION_H_
