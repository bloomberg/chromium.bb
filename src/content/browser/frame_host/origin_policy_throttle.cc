// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/origin_policy_throttle.h"
#include "content/public/browser/origin_policy_commands.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "build/buildflag.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/origin_policy_error_reason.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

namespace {
// Constants derived from the spec, https://github.com/WICG/origin-policy
static constexpr const char* kDeletePolicy = "0";
static constexpr const char* kReportTo = "report-to";
static constexpr const char* kPolicy = "policy";

// Marker for (temporarily) exempted origins.
// TODO(vogelheim): Make sure this is outside the value space for policy
//                  names. A name with a comma in it shouldn't be allowed, but
//                  I don't think we presently check this anywhere.
static constexpr const char* kExemptedOriginPolicyVersion = "exception,";
}  // namespace

namespace content {

// Implement the public "API" from
// content/public/browser/origin_policy_commands.h:
void OriginPolicyAddExceptionFor(const GURL& url) {
  OriginPolicyThrottle::AddExceptionFor(url);
}

// static
bool OriginPolicyThrottle::ShouldRequestOriginPolicy(const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool origin_policy_enabled =
      base::FeatureList::IsEnabled(features::kOriginPolicy) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures);
  if (!origin_policy_enabled)
    return false;

  if (!url.SchemeIs(url::kHttpsScheme))
    return false;

  return true;
}

// static
std::unique_ptr<NavigationThrottle>
OriginPolicyThrottle::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle);

  // We use presence of the origin policy request header to determine
  // whether we should create the throttle.
  if (!handle->GetRequestHeaders().HasHeader(
          net::HttpRequestHeaders::kSecOriginPolicy))
    return nullptr;

  // TODO(vogelheim): Rewrite & hoist up this DCHECK to ensure that ..HasHeader
  //     and ShouldRequestOriginPolicy are always equal on entry to the method.
  //     This depends on https://crbug.com/881234 being fixed.
  DCHECK(OriginPolicyThrottle::ShouldRequestOriginPolicy(handle->GetURL()));
  return base::WrapUnique(new OriginPolicyThrottle(handle));
}

// static
void OriginPolicyThrottle::AddExceptionFor(const GURL& url) {
  GetKnownVersions()[url::Origin::Create(url)] = kExemptedOriginPolicyVersion;
}

OriginPolicyThrottle::~OriginPolicyThrottle() {}

NavigationThrottle::ThrottleCheckResult
OriginPolicyThrottle::WillStartRequest() {
  // TODO(vogelheim): It might be faster in the common case to optimistically
  //     fetch the policy indicated in the request already here. This would
  //     be faster if the last known version is the current version, but
  //     slower (and wasteful of bandwidth) if the server sends us a new
  //     version. It's unclear how much the upside is, though.
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
OriginPolicyThrottle::WillProcessResponse() {
  DCHECK(navigation_handle());

  // Per spec, Origin Policies are only fetched for https:-requests. So we
  // should always have HTTP headers at this point.
  // However, some unit tests generate responses without headers, so we still
  // need to check.
  if (!navigation_handle()->GetResponseHeaders())
    return NavigationThrottle::PROCEED;

  // TODO(andypaicu):
  // This entire logic needs to be moved to OriginPolicyManager with the
  // store migration.

  // This determines whether and which policy version applies and fetches it.
  //
  // Inputs are the kSecOriginPolicy HTTP header, and the version
  // we've last seen from this particular origin.
  //
  // - header with kDeletePolicy received: No policy applies, and delete the
  //       last-known policy for this origin.
  // - header received: Use header version and update last-known policy.
  // - no header received, last-known version exists: Use last-known version
  // - no header, no last-known version: No policy applies.

  std::string response_version =
      GetRequestedPolicyAndReportGroupFromHeader().policy_version;
  bool header_found = !response_version.empty();

  url::Origin origin = GetRequestOrigin();
  DCHECK(!origin.Serialize().empty());
  DCHECK(!origin.opaque());
  KnownVersionMap& versions = GetKnownVersions();
  auto iter = versions.find(origin);

  // Process policy deletion first!
  if (header_found && response_version == kDeletePolicy) {
    if (iter != versions.end())
      versions.erase(iter);
    return NavigationThrottle::PROCEED;
  }

  // Process policy exceptions.
  if (iter != versions.end() && iter->second == kExemptedOriginPolicyVersion) {
    return NavigationThrottle::PROCEED;
  }

  // No policy applies to this request?
  if (!header_found && iter == versions.end()) {
    return NavigationThrottle::PROCEED;
  }

  std::string header;
  navigation_handle()->GetResponseHeaders()->GetNormalizedHeader(
      net::HttpRequestHeaders::kSecOriginPolicy, &header);

  if (!header_found) {
    // TODO(andypaicu):
    // This is an absolute hack that will go away when we move the in-memory
    // store to the network service OriginPolicyManager. Until then, if we have
    // a cached policy version and we receive a request with no header set, we
    // build this artificial header to let OriginPolicyManager know where to
    // retrieve the policy from.
    header = base::StrCat({"policy=", iter->second});
  } else if (iter == versions.end()) {
    versions.insert(std::make_pair(origin, response_version));
  } else {
    iter->second = response_version;
  }

  network::OriginPolicyManager::RetrieveOriginPolicyCallback
      origin_policy_manager_done = base::BindOnce(
          &OriginPolicyThrottle::OnOriginPolicyManagerRetrieveDone,
          base::Unretained(this));
  SiteInstance* site_instance = navigation_handle()->GetStartingSiteInstance();
  StoragePartitionImpl* storage_partition =
      static_cast<StoragePartitionImpl*>(BrowserContext::GetStoragePartition(
          site_instance->GetBrowserContext(), site_instance));

  network::mojom::OriginPolicyManager* origin_policy_manager =
      storage_partition->GetOriginPolicyManagerForBrowserProcess();

  origin_policy_manager->RetrieveOriginPolicy(
      origin, header, std::move(origin_policy_manager_done));

  return NavigationThrottle::DEFER;
}

const char* OriginPolicyThrottle::GetNameForLogging() {
  return "OriginPolicyThrottle";
}

// static
OriginPolicyThrottle::KnownVersionMap&
OriginPolicyThrottle::GetKnownVersionsForTesting() {
  return GetKnownVersions();
}

OriginPolicyThrottle::OriginPolicyThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

OriginPolicyThrottle::KnownVersionMap&
OriginPolicyThrottle::GetKnownVersions() {
  static base::NoDestructor<KnownVersionMap> map_instance;
  return *map_instance;
}

OriginPolicyThrottle::PolicyVersionAndReportTo OriginPolicyThrottle::
    GetRequestedPolicyAndReportGroupFromHeaderStringForTesting(
        const std::string& header) {
  return GetRequestedPolicyAndReportGroupFromHeaderString(header);
}

OriginPolicyThrottle::PolicyVersionAndReportTo
OriginPolicyThrottle::GetRequestedPolicyAndReportGroupFromHeader() const {
  std::string header;
  navigation_handle()->GetResponseHeaders()->GetNormalizedHeader(
      net::HttpRequestHeaders::kSecOriginPolicy, &header);
  return GetRequestedPolicyAndReportGroupFromHeaderString(header);
}

OriginPolicyThrottle::PolicyVersionAndReportTo
OriginPolicyThrottle::GetRequestedPolicyAndReportGroupFromHeaderString(
    const std::string& header) {
  // Compatibility with early spec drafts, for safety reasons:
  // A lonely "0" will be recognized, so that deletion of a policy always works.
  if (net::HttpUtil::TrimLWS(header) == kDeletePolicy)
    return PolicyVersionAndReportTo({kDeletePolicy, ""});

  std::string policy;
  std::string report_to;
  bool valid = true;
  net::HttpUtil::NameValuePairsIterator iter(header.cbegin(), header.cend(),
                                             ',');
  while (iter.GetNext()) {
    std::string token_value =
        net::HttpUtil::TrimLWS(iter.value_piece()).as_string();
    bool is_token = net::HttpUtil::IsToken(token_value);
    if (iter.name_piece() == kPolicy) {
      valid &= is_token && policy.empty();
      policy = token_value;
    } else if (iter.name_piece() == kReportTo) {
      valid &= is_token && report_to.empty();
      report_to = token_value;
    }
  }
  valid &= iter.valid();

  if (!valid)
    return PolicyVersionAndReportTo();
  return PolicyVersionAndReportTo({policy, report_to});
}

const url::Origin OriginPolicyThrottle::GetRequestOrigin() const {
  return url::Origin::Create(navigation_handle()->GetURL());
}

void OriginPolicyThrottle::CancelNavigation(OriginPolicyErrorReason reason,
                                            const GURL& policy_url) {
  base::Optional<std::string> error_page =
      GetContentClient()->browser()->GetOriginPolicyErrorPage(
          reason, navigation_handle());
  CancelDeferredNavigation(NavigationThrottle::ThrottleCheckResult(
      NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page));
  Report(reason, policy_url);
}

#if BUILDFLAG(ENABLE_REPORTING)
void OriginPolicyThrottle::Report(OriginPolicyErrorReason reason,
                                  const GURL& policy_url) {
  const PolicyVersionAndReportTo header_values =
      GetRequestedPolicyAndReportGroupFromHeader();
  if (header_values.report_to.empty())
    return;

  std::string user_agent;
  navigation_handle()->GetRequestHeaders().GetHeader(
      net::HttpRequestHeaders::kUserAgent, &user_agent);

  std::string origin_policy_header;
  navigation_handle()->GetResponseHeaders()->GetNormalizedHeader(
      net::HttpRequestHeaders::kSecOriginPolicy, &origin_policy_header);
  const char* reason_str = nullptr;
  switch (reason) {
    case OriginPolicyErrorReason::kCannotLoadPolicy:
      reason_str = "CANNOT_LOAD";
      break;
    case OriginPolicyErrorReason::kPolicyShouldNotRedirect:
      reason_str = "REDIRECT";
      break;
    case OriginPolicyErrorReason::kOther:
      reason_str = "OTHER";
      break;
  }

  base::DictionaryValue report_body;
  report_body.SetKey("origin_policy_url", base::Value(policy_url.spec()));
  report_body.SetKey("policy", base::Value(origin_policy_header));
  report_body.SetKey("policy_error_reason", base::Value(reason_str));

  SiteInstance* site_instance = navigation_handle()->GetStartingSiteInstance();
  BrowserContext::GetStoragePartition(site_instance->GetBrowserContext(),
                                      site_instance)
      ->GetNetworkContext()
      ->QueueReport("origin-policy", header_values.report_to,
                    navigation_handle()->GetURL(), user_agent,
                    std::move(report_body));
}
#else
void OriginPolicyThrottle::Report(OriginPolicyErrorReason reason,
                                  const GURL& policy_url) {}
#endif  // BUILDFLAG(ENABLE_REPORTING)

bool OriginPolicyThrottle::IsExemptedForTesting(const url::Origin& origin) {
  KnownVersionMap& versions = GetKnownVersions();
  auto iter = versions.find(origin);
  if (iter != versions.end())
    return iter->second == kExemptedOriginPolicyVersion;

  return false;
}

void OriginPolicyThrottle::OnOriginPolicyManagerRetrieveDone(
    const network::mojom::OriginPolicyPtr origin_policy) {
  if (origin_policy->state != network::mojom::OriginPolicyState::kLoaded) {
    CancelNavigation(OriginPolicyErrorReason::kCannotLoadPolicy,
                     origin_policy->policy_url);
    return;
  }

  DCHECK(origin_policy->contents);
  // TODO(vogelheim): Determine whether we need to parse or sanity check
  //                  the policy content at this point.
  static_cast<NavigationHandleImpl*>(navigation_handle())
      ->navigation_request()
      ->SetOriginPolicy(origin_policy->contents->raw_policy);
  Resume();
}

}  // namespace content
