// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/local_heuristics.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/reputation/safety_tips_config.h"
#include "chrome/common/chrome_features.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/security_state/core/features.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace {

const base::FeatureParam<bool> kEnableLookalikeTopSites{
    &security_state::features::kSafetyTipUI, "topsites", true};
const base::FeatureParam<bool> kEnableLookalikeEditDistance{
    &security_state::features::kSafetyTipUI, "editdistance", true};
const base::FeatureParam<bool> kEnableLookalikeEditDistanceSiteEngagement{
    &security_state::features::kSafetyTipUI, "editdistance_siteengagement",
    true};
const base::FeatureParam<bool> kEnableLookalikeTargetEmbedding{
    &security_state::features::kSafetyTipUI, "targetembedding", true};

}  // namespace

bool ShouldTriggerSafetyTipFromLookalike(
    const GURL& url,
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    GURL* safe_url) {
  std::string matched_domain;
  LookalikeUrlMatchType match_type;

  // If the domain and registry is empty, this is a private domain and thus
  // should never be flagged as malicious.
  if (navigated_domain.domain_and_registry.empty()) {
    return false;
  }

  auto* config = GetSafetyTipsRemoteConfigProto();
  const LookalikeTargetAllowlistChecker in_target_allowlist =
      base::BindRepeating(&IsTargetUrlAllowlistedBySafetyTipsComponent, config);
  if (!GetMatchingDomain(navigated_domain, engaged_sites, in_target_allowlist,
                         &matched_domain, &match_type)) {
    return false;
  }

  // If we're already displaying an interstitial, don't warn again.
  if (base::FeatureList::IsEnabled(
          features::kLookalikeUrlNavigationSuggestionsUI) &&
      ShouldBlockLookalikeUrlNavigation(match_type, navigated_domain)) {
    return false;
  }

  *safe_url = GURL(std::string(url::kHttpScheme) +
                   url::kStandardSchemeSeparator + matched_domain);
  switch (match_type) {
    case LookalikeUrlMatchType::kEditDistance:
      return kEnableLookalikeEditDistance.Get();
    case LookalikeUrlMatchType::kEditDistanceSiteEngagement:
      return kEnableLookalikeEditDistanceSiteEngagement.Get();
    case LookalikeUrlMatchType::kTargetEmbedding:
      return kEnableLookalikeTargetEmbedding.Get();
    case LookalikeUrlMatchType::kSiteEngagement:
    case LookalikeUrlMatchType::kSkeletonMatchTop500:
      // We should only ever reach these cases when the lookalike interstitial
      // is disabled. Now that interstitial is fully launched, this only happens
      // in tests.
      DCHECK(!base::FeatureList::IsEnabled(
          features::kLookalikeUrlNavigationSuggestionsUI));
      return true;
    case LookalikeUrlMatchType::kSkeletonMatchTop5k:
      return kEnableLookalikeTopSites.Get();
    case LookalikeUrlMatchType::kNone:
      NOTREACHED();
  }

  NOTREACHED();
  return false;
}

bool ShouldTriggerSafetyTipFromKeywordInURL(
    const GURL& url,
    const DomainInfo& navigated_domain,
    const char* const sensitive_keywords[],
    const size_t num_sensitive_keywords) {
  // We never want to trigger this heuristic on any non-http / https sites.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  std::string eTLD_plus_one = navigated_domain.domain_and_registry;

  // The URL's eTLD + 1 will be empty whenever we're given a host that's
  // invalid.
  if (eTLD_plus_one.empty()) {
    return false;
  }

  size_t registry_length = net::registry_controlled_domains::GetRegistryLength(
      url, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

  // Getting a registry length of 0 means that our URL has an unknown registry.
  if (registry_length == 0) {
    return false;
  }

  // "eTLD + 1 - 1": "www.google.com" -> "google.com" -> "google".
  std::string eTLD_plusminus =
      eTLD_plus_one.substr(0, eTLD_plus_one.size() - registry_length - 1);

  // We should never end up with a "." in our eTLD + 1 - 1.
  DCHECK_EQ(eTLD_plusminus.find("."), std::string::npos);
  // Any problems that would result in an empty eTLD + 1 - 1 should have been
  // caught via the |eTLD_plus_one| check.

  const std::vector<std::string> eTLD_plusminus_parts = base::SplitString(
      eTLD_plusminus, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // We only care about finding a keyword here if there's more than one part to
  // the tokenized eTLD + 1 - 1.
  if (eTLD_plusminus_parts.size() <= 1) {
    return false;
  }

  for (const auto& eTLD_plusminus_part : eTLD_plusminus_parts) {
    // We use a custom comparator for (char *) here, to avoid the costly
    // construction of two std::strings every time two values are compared,
    // and because (char *) orders by address, not lexicographically.
    if (std::binary_search(sensitive_keywords,
                           sensitive_keywords + num_sensitive_keywords,
                           eTLD_plusminus_part.c_str(),
                           [](const char* str_one, const char* str_two) {
                             return strcmp(str_one, str_two) < 0;
                           })) {
      return true;
    }
  }

  return false;
}
