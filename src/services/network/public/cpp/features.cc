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
  "NetworkService",
#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(IS_CHROMECAST))
      base::FEATURE_ENABLED_BY_DEFAULT
};
#else
      base::FEATURE_DISABLED_BY_DEFAULT
};
#endif

// Out of Blink CORS
const base::Feature kOutOfBlinkCors{"OutOfBlinkCors",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

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

// Kill switch for enforcing
// URLLoaderFactoryParams::request_initiator_origin_lock for Cross-Origin Read
// Blocking.  When enabled, then CORB treats |request_initiator| as opaque
// when it doesn't match |request_initiator_site_lock|.
const base::Feature kEnforceRequestInitiatorLockForCorb{
    "EnforceRequestInitiatorLockForCorb", base::FEATURE_ENABLED_BY_DEFAULT};

// Implementation of https://mikewest.github.io/sec-metadata/
const base::Feature kFetchMetadata{"FetchMetadata",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// The `Sec-Fetch-Dest` header is split out from the main "FetchMetadata"
// feature so we can ship the broader feature without this specifific bit
// while we continue discussion.
const base::Feature kFetchMetadataDestination{
    "FetchMetadataDestination", base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldEnableOutOfBlinkCors() {
  // OOR-CORS requires NetworkService.
  if (!base::FeatureList::IsEnabled(features::kNetworkService))
    return false;

  return base::FeatureList::IsEnabled(features::kOutOfBlinkCors);
}

}  // namespace features
}  // namespace network
