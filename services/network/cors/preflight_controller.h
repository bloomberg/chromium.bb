// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_
#define SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_

#include <memory>

#include "base/component_export.h"
#include "services/network/public/cpp/resource_request.h"

namespace network {

namespace cors {

// A class to manage CORS-preflight, making a CORS-preflight request, checking
// its result, and owning a CORS-preflight cache.
// TODO(toyoshim): Features explained above not fully implemented yet.
// See also https://crbug.com/803766 to check a design doc.
class COMPONENT_EXPORT(NETWORK_SERVICE) PreflightController {
 public:
  // Creates a CORS-preflight ResourceRequest for a specified |request| for a
  // URL that is originally requested.
  // Note: This function is exposed for testing only purpose, and production
  // code outside this class should not call this function directly.
  static std::unique_ptr<ResourceRequest> CreatePreflightRequest(
      const ResourceRequest& request);

  // TODO(toyoshim): Implements an asynchronous interface to consult about
  // CORS-preflight check, that manages a preflight cache, and may make a
  // preflight request internally.
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_PREFLIGHT_CONTROLLER_H_
