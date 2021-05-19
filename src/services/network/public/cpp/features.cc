// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

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

// When kPauseBrowserInitiatedHeavyTrafficForP2P is enabled, then a subset of
// the browser initiated traffic may be paused if there is at least one active
// P2P connection and the network is estimated to be congested. This feature is
// intended to throttle only the browser initiated traffic that is expected to
// be heavy (has large request/response sizes) when real time content might be
// streaming over an active P2P connection.
const base::Feature kPauseBrowserInitiatedHeavyTrafficForP2P{
    "PauseBrowserInitiatedHeavyTrafficForP2P",
    base::FEATURE_ENABLED_BY_DEFAULT};

// When kPauseLowPriorityBrowserRequestsOnWeakSignal is enabled, then a subset
// of the browser initiated requests may be deferred if the device is using
// cellular connection and the signal quality is low. Android only.
const base::Feature kPauseLowPriorityBrowserRequestsOnWeakSignal{
    "PauseLowPriorityBrowserRequestsOnWeakSignal",
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

// Enables Cross-Origin-Embedder-Policy: cors-or-credentialless.
// https://github.com/mikewest/credentiallessness
COMPONENT_EXPORT(NETWORK_CPP)
extern const base::Feature kCrossOriginEmbedderPolicyCredentialless{
    "CrossOriginEmbedderPolicyCredentialless",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Cross-Origin Opener Policy (COOP).
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
// https://html.spec.whatwg.org/#cross-origin-opener-policy
// Currently this feature is enabled for all platforms except WebView.
const base::Feature kCrossOriginOpenerPolicy{"CrossOriginOpenerPolicy",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Cross-Origin-Opener-Policy reporting API origin trial. It will be
// used as a kill switch during the experiment.
const base::Feature kCrossOriginOpenerPolicyReportingOriginTrial{
    "CrossOriginOpenerPolicyReportingOriginTrial",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Cross-Origin Opener Policy (COOP) reporting.
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
const base::Feature kCrossOriginOpenerPolicyReporting{
    "CrossOriginOpenerPolicyReporting", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Cross-Origin Opener Policy (COOP) access reporting.
// https://github.com/camillelamy/explainers/blob/master/coop_reporting.md#report-blocked-accesses-to-other-windows
const base::Feature kCrossOriginOpenerPolicyAccessReporting{
    "CrossOriginOpenerPolicyAccessReporting", base::FEATURE_ENABLED_BY_DEFAULT};

// Shift's COOP's default from `unsafe-none` to `same-origin-allow-popups`.
// https://github.com/mikewest/coop-by-default/
const base::Feature kCrossOriginOpenerPolicyByDefault{
    "CrossOriginOpenerPolicyByDefault", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the most recent developments on the crossOriginIsolated property.
// https://developer.mozilla.org/en-US/docs/Web/API/WindowOrWorkerGlobalScope/crossOriginIsolated
const base::Feature kCrossOriginIsolated{"CrossOriginIsolated",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or defaults splittup up server (not proxy) entries in the
// HttpAuthCache.
const base::Feature kSplitAuthCacheByNetworkIsolationKey{
    "SplitAuthCacheByNetworkIsolationKey", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable usage of hardcoded DoH upgrade mapping for use in automatic mode.
const base::Feature kDnsOverHttpsUpgrade {
  "DnsOverHttpsUpgrade",
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC) || defined(OS_ANDROID) || \
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

// Controls whether a |request_initiator| that mismatches
// |request_initiator_origin_lock| leads to 1) failing the HTTP request and 2)
// calling mojo::ReportBadMessage (on desktop platforms, where NetworkService
// is hosted outside of the Browser process, this leads to DumpWithoutCrashing
// and does *not* lead to a renderer kill).
//
// See also https://crbug.com/920634
const base::Feature kRequestInitiatorSiteLockEnfocement = {
    "RequestInitiatorSiteLockEnfocement",
    base::FEATURE_ENABLED_BY_DEFAULT};

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

// Determines whether Trust Tokens issuance requests should be diverted, at the
// corresponding issuers' request, to the operating system instead of sent
// to the issuers' servers.
//
// WARNING: If you rename this param, you must update the corresponding flag
// entry in about_flags.cc.
const base::FeatureParam<bool> kPlatformProvidedTrustTokenIssuance{
    &kTrustTokens, "PlatformProvidedTrustTokenIssuance", false};

const base::Feature kWebSocketReassembleShortMessages{
    "WebSocketReassembleShortMessages", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable support for ACCEPT_CH H2/3 frame as part of Client Hint Reliability.
// See:
// https://tools.ietf.org/html/draft-davidben-http-client-hint-reliability-02#section-4.3
const base::Feature kAcceptCHFrame{"AcceptCHFrame",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace network
