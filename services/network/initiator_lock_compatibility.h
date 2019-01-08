// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_INITIATOR_LOCK_COMPATIBILITY_H_
#define SERVICES_NETWORK_INITIATOR_LOCK_COMPATIBILITY_H_

#include "base/component_export.h"
#include "base/optional.h"
#include "url/origin.h"

namespace network {

struct ResourceRequest;

namespace mojom {
class URLLoaderFactoryParams;
}  // namespace mojom

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "RequestInitiatorOriginLockCompatibility" in
// tools/metrics/histograms/enums.xml.
enum class InitiatorLockCompatibility {
  // Request came from a browser process and so the
  // |request_initiator_site_lock| doesn't apply.
  kBrowserProcess = 0,

  // |request_initiator_site_lock| is missing - see https://crbug.com/891872
  // and RenderProcessHostImpl::CreateURLLoaderFactoryWithOptionalOrigin.
  kNoLock = 1,

  // |request_initiator| is missing.
  kNoInitiator = 2,

  // |request.request_initiator| is compatible with
  // |factory_params_.request_initiator_site_lock| - either
  // |request.request_initiator| is opaque or it is equal to
  // |request_initiator_site_lock|.
  kCompatibleLock = 3,

  // |request.request_initiator| is incompatible with
  // |factory_params_.request_initiator_site_lock|.  Cases known so far where
  // this can occur:
  // - HTML Imports (see https://crbug.com/871827#c9).
  kIncorrectLock = 4,

  kMaxValue = kIncorrectLock,
};

// Verifies if |request.request_initiator| matches
// |factory_params.request_initiator_site_lock|.
COMPONENT_EXPORT(NETWORK_SERVICE)
InitiatorLockCompatibility VerifyRequestInitiatorLock(
    const mojom::URLLoaderFactoryParams& factory_params,
    const ResourceRequest& request);
COMPONENT_EXPORT(NETWORK_SERVICE)
InitiatorLockCompatibility VerifyRequestInitiatorLock(
    const base::Optional<url::Origin>& request_initiator_site_lock,
    const base::Optional<url::Origin>& request_initiator);

}  // namespace network

#endif  // SERVICES_NETWORK_INITIATOR_LOCK_COMPATIBILITY_H_
