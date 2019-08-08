// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/initiator_lock_compatibility.h"

#include <string>

#include "base/feature_list.h"
#include "base/logging.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace network {

InitiatorLockCompatibility VerifyRequestInitiatorLock(
    const base::Optional<url::Origin>& request_initiator_site_lock,
    const base::Optional<url::Origin>& request_initiator) {
  if (!request_initiator_site_lock.has_value())
    return InitiatorLockCompatibility::kNoLock;
  const url::Origin& lock = request_initiator_site_lock.value();

  if (!request_initiator.has_value()) {
    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      // SECURITY CHECK: Renderer processes should always provide a
      // |request_initiator|.  Similarly, browser-side features acting on
      // behalf of a renderer process (like AppCache), should always provide a
      // |request_initiator|.
      //
      // Callers associated with features (e.g. Sec-Fetch-Site) that may handle
      // browser-initiated requests (e.g. navigations and/or SafeBrowsing
      // traffic) need to take extra care to avoid calling
      // VerifyRequestInitiatorLock (and/or GetTrustworthyInitiator) with no
      // |request_initiator|.  Such features should return early when handling
      // request with no |request_initiator| but only when the request comes
      // through a URLLoaderFactory associated with kBrowserProcessId.
      CHECK(false);
    }
    return InitiatorLockCompatibility::kNoInitiator;
  }
  const url::Origin& initiator = request_initiator.value();

  // TODO(lukasza, nasko): Also consider equality of precursor origins (e.g. if
  // |initiator| is opaque, then it's precursor origin should match the |lock|
  // [or |lock|'s precursor if |lock| is also opaque]).
  if (initiator.opaque() || (initiator == lock))
    return InitiatorLockCompatibility::kCompatibleLock;

  // TODO(lukasza, nasko): https://crbug.com/888079: Return kIncorrectLock if
  // the origins do not match exactly in the previous if statement.  This should
  // be possible to do once we no longer fall back to site_url and have
  // request_initiator_*origin*_lock instead.  In practice, the fallback can go
  // away after we no longer vend process-wide factory: https://crbug.com/891872
  if (!initiator.opaque() && !lock.opaque() &&
      initiator.scheme() == lock.scheme() &&
      initiator.GetURL().SchemeIsHTTPOrHTTPS() &&
      !initiator.GetURL().HostIsIPAddress()) {
    std::string lock_domain = lock.host();
    if (!lock_domain.empty() && lock_domain.back() == '.')
      lock_domain.erase(lock_domain.length() - 1);
    if (initiator.DomainIs(lock_domain))
      return InitiatorLockCompatibility::kCompatibleLock;
  }

  return InitiatorLockCompatibility::kIncorrectLock;
}

url::Origin GetTrustworthyInitiator(
    const base::Optional<url::Origin>& request_initiator_site_lock,
    const base::Optional<url::Origin>& request_initiator) {
  // Returning a unique origin as a fallback should be safe - such origin will
  // be considered cross-origin from all other origins.
  url::Origin unique_origin_fallback;

  if (!request_initiator.has_value())
    return unique_origin_fallback;

  if (!base::FeatureList::IsEnabled(features::kNetworkService) ||
      !base::FeatureList::IsEnabled(features::kRequestInitiatorSiteLock)) {
    return request_initiator.value();
  }

  InitiatorLockCompatibility initiator_compatibility =
      VerifyRequestInitiatorLock(request_initiator_site_lock,
                                 request_initiator);
  if (initiator_compatibility == InitiatorLockCompatibility::kIncorrectLock)
    return unique_origin_fallback;

  // If all the checks above passed, then |request_initiator| is trustworthy.
  return request_initiator.value();
}

}  // namespace network
