// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/features.h"

#include "build/build_config.h"

namespace network {
namespace features {

// When kCapReferrerToOriginOnCrossOrigin is enabled, HTTP referrers on cross-
// origin requests are restricted to contain at most the source origin.
const base::Feature kCapReferrerToOriginOnCrossOrigin{
    "CapReferrerToOriginOnCrossOrigin", base::FEATURE_DISABLED_BY_DEFAULT};

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

// Out of Blink CORS for browsers is launched at m79 (http://crbug.com/1001450),
// and one for WebView will be at m81 (http://crbug.com/1035763).
// The legacy CORS will be also maintained at least until m81 for enterprise
// users. See https://sites.google.com/a/chromium.org/dev/Home/loading/oor-cors
// for FYI Builders information.
const base::Feature kOutOfBlinkCors{"OutOfBlinkCors",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

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
    base::FEATURE_ENABLED_BY_DEFAULT};

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

// Enables Cross-Origin Opener Policy (COOP).
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
// Currently this feature is enabled for all platforms except WebView. It is not
// possible to distinguish between Android and WebView here, so we enable the
// feature on Android via finch.
const base::Feature kCrossOriginOpenerPolicy {
  "CrossOriginOpenerPolicy",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enables Cross-Origin Opener Policy (COOP) reporting.
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
const base::Feature kCrossOriginOpenerPolicyReporting{
    "CrossOriginOpenerPolicyReporting", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Cross-Origin Embedder Policy (COEP).
// https://github.com/mikewest/corpp
// Currently this feature is enabled for all platforms except WebView.
const base::Feature kCrossOriginEmbedderPolicy{
    "CrossOriginEmbedderPolicy", base::FEATURE_ENABLED_BY_DEFAULT};

// When kBlockNonSecureExternalRequests is enabled, requests initiated from a
// pubic network may only target a private network if the initiating context
// is secure.
//
// https://wicg.github.io/cors-rfc1918/#integration-fetch
const base::Feature kBlockNonSecureExternalRequests{
    "BlockNonSecureExternalRequests", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or defaults splittup up server (not proxy) entries in the
// HttpAuthCache.
const base::Feature kSplitAuthCacheByNetworkIsolationKey{
    "SplitAuthCacheByNetworkIsolationKey", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable usage of hardcoded DoH upgrade mapping for use in automatic mode.
const base::Feature kDnsOverHttpsUpgrade {
  "DnsOverHttpsUpgrade",
#if defined(OS_CHROMEOS) || defined(OS_MACOSX) || defined(OS_ANDROID) || \
    defined(OS_WIN)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// If this feature is enabled, the mDNS responder service responds to queries
// for TXT records associated with
// "Generated-Names._mdns_name_generator._udp.local" with a list of generated
// mDNS names (random UUIDs) in the TXT record data.
const base::Feature kMdnsResponderGeneratedNameListing{
    "MdnsResponderGeneratedNameListing", base::FEATURE_DISABLED_BY_DEFAULT};

// Provides a mechanism to disable DoH upgrades for some subset of the hardcoded
// upgrade mapping. Separate multiple provider ids with commas. See the
// mapping in net/dns/dns_util.cc for provider ids.
const base::FeatureParam<std::string>
    kDnsOverHttpsUpgradeDisabledProvidersParam{&kDnsOverHttpsUpgrade,
                                               "DisabledProviders", ""};

// Disable special treatment on requests with keepalive set (see
// https://fetch.spec.whatwg.org/#request-keepalive-flag). This is introduced
// for investigation on the memory usage, and should not be enabled widely.
const base::Feature kDisableKeepaliveFetch{"DisableKeepaliveFetch",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// When kOutOfBlinkFrameAncestors is enabled, the frame-ancestors
// directive is parsed from the Content-Security-Policy header in the network
// service and enforced in the browser.
const base::Feature kOutOfBlinkFrameAncestors{"OutOfBlinkFrameAncestors",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Attach the origin of the destination URL to the "origin" header
const base::Feature
    kDeriveOriginFromUrlForNeitherGetNorHeadRequestWhenHavingSpecialAccess{
        "DeriveOriginFromUrlForNeitherGetNorHeadRequestWhenHavingSpecialAccess",
        base::FEATURE_DISABLED_BY_DEFAULT};

// Emergency switch for legacy cookie access semantics on given patterns, as
// specified by the param, comma separated.
const base::Feature kEmergencyLegacyCookieAccess{
    "EmergencyLegacyCookieAccess", base::FEATURE_DISABLED_BY_DEFAULT};
const char kEmergencyLegacyCookieAccessParamName[] = "Patterns";
const base::FeatureParam<std::string> kEmergencyLegacyCookieAccessParam{
    &kEmergencyLegacyCookieAccess, kEmergencyLegacyCookieAccessParamName, ""};

// Controls whether the CORB allowlist [1] is also applied to OOR-CORS (e.g.
// whether non-allowlisted content scripts are subject to CORS in OOR-CORS
// mode).  See also: https://crbug.com/920638
//
// [1]
// https://www.chromium.org/Home/chromium-security/extension-content-script-fetches
const base::Feature kCorbAllowlistAlsoAppliesToOorCors = {
    "CorbAllowlistAlsoAppliesToOorCors", base::FEATURE_DISABLED_BY_DEFAULT};
const char kCorbAllowlistAlsoAppliesToOorCorsParamName[] =
    "AllowlistForCorbAndCors";

// The preflight parser should reject Access-Control-Allow-* headers which do
// not conform to ABNF. But if the strict check is applied directly, some
// existing sites might fail to load. The feature flag controls whether a strict
// check will be used or not.
const base::Feature kStrictAccessControlAllowListCheck = {
    "StrictAccessControlAllowListCheck", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables preprocessing requests with the Trust Tokens API Fetch flags set,
// and handling their responses, according to the protocol.
// (See https://github.com/WICG/trust-token-api.)
const base::Feature kTrustTokens{"TrustTokens",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// Determines which Trust Tokens operations require the TrustTokens origin trial
// active in order to be used. This is runtime-configurable so that the Trust
// Tokens operations of issuance, redemption, and signing are compatible with
// both standard origin trials and third-party origin trials:
//
// - For standard origin trials, set kOnlyIssuanceRequiresOriginTrial. In Blink,
// all of the interface will be enabled (so long as the base::Feature is!), and
// issuance operations will check at runtime if the origin trial is enabled,
// returning an error if it is not.
// - For third-party origin trials, set kAllOperationsRequireOriginTrial. In
// Blink, the interface will be enabled exactly when the origin trial is present
// in the executing context (so long as the base::Feature is present).
//
// For testing, set kOriginTrialNotRequired. With this option, although all
// operations will still only be available if the base::Feature is enabled, none
// will additionally require that the origin trial be active.
const base::FeatureParam<TrustTokenOriginTrialSpec>::Option
    kTrustTokenOriginTrialParamOptions[] = {
        {TrustTokenOriginTrialSpec::kOriginTrialNotRequired,
         "origin-trial-not-required"},
        {TrustTokenOriginTrialSpec::kAllOperationsRequireOriginTrial,
         "all-operations-require-origin-trial"},
        {TrustTokenOriginTrialSpec::kOnlyIssuanceRequiresOriginTrial,
         "only-issuance-requires-origin-trial"}};
const base::FeatureParam<TrustTokenOriginTrialSpec>
    kTrustTokenOperationsRequiringOriginTrial{
        &kTrustTokens, "TrustTokenOperationsRequiringOriginTrial",
        TrustTokenOriginTrialSpec::kOriginTrialNotRequired,
        &kTrustTokenOriginTrialParamOptions};

bool ShouldEnableOutOfBlinkCorsForTesting() {
  return base::FeatureList::IsEnabled(features::kOutOfBlinkCors);
}

}  // namespace features
}  // namespace network
