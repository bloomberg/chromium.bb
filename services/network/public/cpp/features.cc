// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/features.h"

#include "build/build_config.h"

namespace network {
namespace features {

// Enables Expect CT reporting, which sends reports for opted-in sites
// that don't serve sufficient Certificate Transparency information.
const base::Feature kExpectCTReporting{"ExpectCTReporting",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNetworkErrorLogging{"NetworkErrorLogging",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
// Enables the network service.
const base::Feature kNetworkService {
#if defined(OS_ANDROID)
  "NetworkService",
#else
  "NetworkServiceNotSupported",
#endif
      base::FEATURE_ENABLED_BY_DEFAULT
};

// Out of Blink CORS
const base::Feature kOutOfBlinkCors {
  "OutOfBlinkCors",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
};
#else
      base::FEATURE_ENABLED_BY_DEFAULT
};
#endif

const base::Feature kReporting{"Reporting", base::FEATURE_ENABLED_BY_DEFAULT};

// Based on the field trial parameters, this feature will override the value of
// the maximum number of delayable requests allowed in flight. The number of
// delayable requests allowed in flight will be based on the network's
// effective connection type ranges and the
// corresponding number of delayable requests in flight specified in the
// experiment configuration. Based on field trial parameters, this experiment
// may also throttle delayable requests based on the number of non-delayable
// requests in-flight times a weighting factor.
const base::Feature kThrottleDelayable{"ThrottleDelayable",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// When kPriorityRequestsDelayableOnSlowConnections is enabled, HTTP
// requests fetched from a SPDY/QUIC/H2 proxies can be delayed by the
// ResourceScheduler just as HTTP/1.1 resources are. However, requests from such
// servers are not subject to kMaxNumDelayableRequestsPerHostPerClient limit.
const base::Feature kDelayRequestsOnMultiplexedConnections{
    "DelayRequestsOnMultiplexedConnections", base::FEATURE_ENABLED_BY_DEFAULT};

// Implementation of https://mikewest.github.io/sec-metadata/
const base::Feature kFetchMetadata{"FetchMetadata",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// The `Sec-Fetch-Dest` header is split out from the main "FetchMetadata"
// feature so we can ship the broader feature without this specifific bit
// while we continue discussion.
const base::Feature kFetchMetadataDestination{
    "FetchMetadataDestination", base::FEATURE_DISABLED_BY_DEFAULT};

// When kRequestInitiatorSiteLock is enabled, then CORB, CORP and Sec-Fetch-Site
// will validate network::ResourceRequest::request_initiator against
// network::mojom::URLLoaderFactoryParams::request_initiator_site_lock.
const base::Feature kRequestInitiatorSiteLock{"RequestInitiatorSiteLock",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// When kPauseBrowserInitiatedHeavyTrafficForP2P is enabled, then a subset of
// the browser initiated traffic may be paused if there is at least one active
// P2P connection and the network is estimated to be congested. This feature is
// intended to throttle only the browser initiated traffic that is expected to
// be heavy (has large request/response sizes) when real time content might be
// streaming over an active P2P connection.
const base::Feature kPauseBrowserInitiatedHeavyTrafficForP2P{
    "PauseBrowserInitiatedHeavyTrafficForP2P",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When kCORBProtectionSniffing is enabled CORB sniffs additional same-origin
// resources if they look sensitive.
const base::Feature kCORBProtectionSniffing{"CORBProtectionSniffing",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// When kProactivelyThrottleLowPriorityRequests is enabled,
// resource scheduler proactively throttles low priority requests to avoid
// network contention with high priority requests that may arrive soon.
const base::Feature kProactivelyThrottleLowPriorityRequests{
    "ProactivelyThrottleLowPriorityRequests",
    base::FEATURE_DISABLED_BY_DEFAULT};

// https://github.com/mikewest/corpp
const base::Feature kCrossOriginEmbedderPolicy{
    "CrossOriginEmbedderPolicy", base::FEATURE_DISABLED_BY_DEFAULT};

// If this feature is enabled, the mDNS responder service responds to queries
// for TXT records associated with
// "Generated-Names._mdns_name_generator._udp.local" with a list of generated
// mDNS names (random UUIDs) in the TXT record data.
const base::Feature kMdnsResponderGeneratedNameListing{
    "MdnsResponderGeneratedNameListing", base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldEnableOutOfBlinkCors() {
  // OOR-CORS requires NetworkService.
  if (!base::FeatureList::IsEnabled(features::kNetworkService))
    return false;

  return base::FeatureList::IsEnabled(features::kOutOfBlinkCors);
}

}  // namespace features
}  // namespace network
