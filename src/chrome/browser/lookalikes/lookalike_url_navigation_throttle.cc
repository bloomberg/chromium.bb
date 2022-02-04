// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "chrome/browser/lookalikes/lookalike_url_controller_client.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/lookalike_url_tab_storage.h"
#include "chrome/browser/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reputation/reputation_service.h"
#include "components/lookalikes/core/features.h"
#include "components/lookalikes/core/lookalike_url_ui_util.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/reputation/core/safety_tips_config.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace {

typedef content::NavigationThrottle::ThrottleCheckResult ThrottleCheckResult;

// Returns true if |current_url| is at the end of the redirect chain
// stored in |stored_redirect_chain|.
bool IsInterstitialReload(const GURL& current_url,
                          const std::vector<GURL>& stored_redirect_chain) {
  return stored_redirect_chain.size() > 1 &&
         stored_redirect_chain[stored_redirect_chain.size() - 1] == current_url;
}

const base::Feature kOptimizeLookalikeUrlNavigationThrottle{
    "OptimizeLookalikeUrlNavigationThrottle",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

LookalikeUrlNavigationThrottle::LookalikeUrlNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      profile_(Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())) {}

LookalikeUrlNavigationThrottle::~LookalikeUrlNavigationThrottle() {}

ThrottleCheckResult LookalikeUrlNavigationThrottle::WillStartRequest() {
  if (profile_->AsTestingProfile())
    return content::NavigationThrottle::PROCEED;

  auto* service = LookalikeUrlService::Get(profile_);
  if (base::FeatureList::IsEnabled(kOptimizeLookalikeUrlNavigationThrottle) &&
      service->EngagedSitesNeedUpdating()) {
    service->ForceUpdateEngagedSites(base::DoNothing());
  }
  return content::NavigationThrottle::PROCEED;
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::WillProcessResponse() {
  // Ignore if running unit tests. Some tests use
  // TestMockTimeTaskRunner::ScopedContext and call CreateTestWebContents()
  // which navigates and waits for throttles to complete using a RunLoop.
  // However, TestMockTimeTaskRunner::ScopedContext disallows RunLoop so those
  // tests crash. We should only do this with a real profile anyways.
  // use_test_profile is set by unit tests to true so that the rest of the
  // throttle is exercised.
  // In other words, this condition is false in production code, browser tests
  // and only lookalike unit tests. It's true in all non-lookalike unit tests.
  if (!use_test_profile_ && profile_->AsTestingProfile()) {
    return content::NavigationThrottle::PROCEED;
  }

  content::NavigationHandle* handle = navigation_handle();

  // Ignore errors, subframe and same document navigations.
  if (handle->GetNetErrorCode() != net::OK || !handle->IsInMainFrame() ||
      handle->IsSameDocument()) {
    return content::NavigationThrottle::PROCEED;
  }

  // Get stored interstitial parameters early. By doing so, we ensure that a
  // navigation to an irrelevant (for this interstitial's purposes) URL such as
  // chrome://settings while the lookalike interstitial is being shown clears
  // the stored state:
  // 1. User navigates to lookalike.tld which redirects to site.tld.
  // 2. Interstitial shown.
  // 3. User navigates to chrome://settings.
  // If, after this, the user somehow ends up on site.tld with a reload (e.g.
  // with ReloadType::ORIGINAL_REQUEST_URL), this will correctly not show an
  // interstitial.
  LookalikeUrlTabStorage* tab_storage =
      LookalikeUrlTabStorage::GetOrCreate(handle->GetWebContents());
  const LookalikeUrlTabStorage::InterstitialParams interstitial_params =
      tab_storage->GetInterstitialParams();
  tab_storage->ClearInterstitialParams();

  // If this is a reload and if the current URL is the last URL of the stored
  // redirect chain, the interstitial was probably reloaded. Stop the reload and
  // navigate back to the original lookalike URL so that the whole throttle is
  // exercised again.
  if (handle->GetReloadType() != content::ReloadType::NONE &&
      IsInterstitialReload(handle->GetURL(),
                           interstitial_params.redirect_chain)) {
    CHECK(interstitial_params.url.SchemeIsHTTPOrHTTPS());
    // See
    // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/plIZV3Rkzok
    // for why this is OK. Assume interstitial reloads are always browser
    // initiated.
    handle->GetWebContents()->OpenURL(content::OpenURLParams(
        interstitial_params.url, interstitial_params.referrer,
        WindowOpenDisposition::CURRENT_TAB,
        ui::PageTransition::PAGE_TRANSITION_RELOAD,
        false /* is_renderer_initiated */));
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  LookalikeUrlService* service = LookalikeUrlService::Get(profile_);
  if (!use_test_profile_ && service->EngagedSitesNeedUpdating()) {
    service->ForceUpdateEngagedSites(
        base::BindOnce(&LookalikeUrlNavigationThrottle::PerformChecksDeferred,
                       weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
    return content::NavigationThrottle::DEFER;
  }
  return PerformChecks(service->GetLatestEngagedSites());
}

const char* LookalikeUrlNavigationThrottle::GetNameForLogging() {
  return "LookalikeUrlNavigationThrottle";
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::ShowInterstitial(
    const GURL& safe_domain,
    const GURL& lookalike_domain,
    ukm::SourceId source_id,
    LookalikeUrlMatchType match_type,
    bool triggered_by_initial_url) {
  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* web_contents = handle->GetWebContents();

  auto controller = std::make_unique<LookalikeUrlControllerClient>(
      web_contents, lookalike_domain, safe_domain);

  std::unique_ptr<LookalikeUrlBlockingPage> blocking_page(
      new LookalikeUrlBlockingPage(
          web_contents, safe_domain, lookalike_domain, source_id, match_type,
          handle->IsSignedExchangeInnerResponse(), triggered_by_initial_url,
          std::move(controller)));

  absl::optional<std::string> error_page_contents =
      blocking_page->GetHTMLContents();

  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      handle, std::move(blocking_page));

  // Store interstitial parameters in per-tab storage. Reloading the
  // interstitial once it's shown navigates to the final URL in the original
  // redirect chain. It also loses the original redirect chain. By storing these
  // parameters, we can check if the next navigation is a reload and act
  // accordingly.
  content::Referrer referrer(handle->GetReferrer().url,
                             handle->GetReferrer().policy);
  LookalikeUrlTabStorage::GetOrCreate(handle->GetWebContents())
      ->OnLookalikeInterstitialShown(lookalike_domain, referrer,
                                     handle->GetRedirectChain());
  return ThrottleCheckResult(content::NavigationThrottle::CANCEL,
                             net::ERR_BLOCKED_BY_CLIENT, error_page_contents);
}

std::unique_ptr<LookalikeUrlNavigationThrottle>
LookalikeUrlNavigationThrottle::MaybeCreateNavigationThrottle(
    content::NavigationHandle* navigation_handle) {
  // If the tab is being no-state prefetched, stop here before it breaks
  // metrics.
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  if (prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents)) {
    return nullptr;
  }

  // Otherwise, always insert the throttle for metrics recording.
  return std::make_unique<LookalikeUrlNavigationThrottle>(navigation_handle);
}

ThrottleCheckResult
LookalikeUrlNavigationThrottle::CheckManifestsAndMaybeShowInterstitial(
    const GURL& safe_domain,
    const GURL& lookalike_domain,
    ukm::SourceId source_id,
    LookalikeUrlMatchType match_type,
    bool triggered_by_initial_url) {
  RecordUMAFromMatchType(match_type);

  // Punycode interstitial doesn't have a target site, so safe_domain isn't
  // valid.
  if (!base::FeatureList::IsEnabled(
          lookalikes::features::kLookalikeDigitalAssetLinks) ||
      !safe_domain.is_valid()) {
    return ShowInterstitial(safe_domain, lookalike_domain, source_id,
                            match_type, triggered_by_initial_url);
  }

  const url::Origin lookalike_origin =
      url::Origin::Create(navigation_handle()->GetURL());
  const url::Origin target_origin = url::Origin::Create(safe_domain);
  DigitalAssetLinkCrossValidator::ResultCallback callback = base::BindOnce(
      &LookalikeUrlNavigationThrottle::OnManifestValidationResult,
      weak_factory_.GetWeakPtr(), safe_domain, lookalike_domain, source_id,
      match_type, triggered_by_initial_url);
  DCHECK(!digital_asset_link_validator_);
  // This assumes each navigation has its own throttle.
  // TODO(crbug.com/1175385): Consider moving this to LookalikeURLService.
  digital_asset_link_validator_ =
      std::make_unique<DigitalAssetLinkCrossValidator>(
          profile_, lookalike_origin, target_origin,
          LookalikeUrlService::kManifestFetchDelay.Get(),
          LookalikeUrlService::Get(profile_)->clock(), std::move(callback));
  digital_asset_link_validator_->Start();
  return NavigationThrottle::DEFER;
}

void LookalikeUrlNavigationThrottle::OnManifestValidationResult(
    const GURL& safe_domain,
    const GURL& lookalike_domain,
    ukm::SourceId source_id,
    LookalikeUrlMatchType match_type,
    bool triggered_by_initial_url,
    bool validation_succeeded) {
  if (validation_succeeded) {
    // Add the lookalike URL to the allowlist.
    // TODO(meacer): Use a proper key for caching here. At the very least, we
    // should allowlist (lookalike, target) pairs. We should also cache some of
    // the failure cases, e.g. when the lookalike site serves a manifest but it
    // doesn't have an entry for the target site.
    ReputationService::Get(profile_)->SetUserIgnore(lookalike_domain);

    Resume();
    return;
  }
  ThrottleCheckResult result =
      ShowInterstitial(safe_domain, lookalike_domain, source_id, match_type,
                       triggered_by_initial_url);
  CancelDeferredNavigation(result);
}

void LookalikeUrlNavigationThrottle::PerformChecksDeferred(
    base::TimeTicks start,
    const std::vector<DomainInfo>& engaged_sites) {
  UMA_HISTOGRAM_TIMES("NavigationSuggestion.UpdateEngagedSitesDeferTime",
                      base::TimeTicks::Now() - start);
  ThrottleCheckResult result = PerformChecks(engaged_sites);
  if (result.action() == NavigationThrottle::DEFER) {
    // Already deferred by PerformChecks(), don't defer again. PerformChecks()
    // is responsible for scheduling the cancellation/resumption of the
    // navigation.
    return;
  }
  if (result.action() == NavigationThrottle::PROCEED) {
    Resume();
    return;
  }
  CancelDeferredNavigation(result);
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::PerformChecks(
    const std::vector<DomainInfo>& engaged_sites) {
  // The last URL in the redirect chain must be the same as the commit URL,
  // or the navigation is a loadData navigation (where the base URL is saved in
  // the redirect chain, instead of the commit URL).
  const GURL& last_url_in_redirect_chain =
      navigation_handle()
          ->GetRedirectChain()[navigation_handle()->GetRedirectChain().size() -
                               1];
  DCHECK(last_url_in_redirect_chain == navigation_handle()->GetURL() ||
         !navigation_handle()->GetBaseURLForDataURL().is_empty());

  // Check for two lookalikes -- at the beginning and end of the redirect chain.
  const GURL& first_url = navigation_handle()->GetRedirectChain()[0];
  const GURL& last_url = navigation_handle()->GetURL();

  // If first_url and last_url share a hostname, then only check last_url.
  // This saves time, and avoids clouding metrics.
  LookalikeUrlMatchType first_match_type;
  GURL first_suggested_url;
  bool first_is_lookalike =
      first_url.host() != last_url.host() &&
      IsLookalikeUrl(first_url, engaged_sites, &first_match_type,
                     &first_suggested_url);

  LookalikeUrlMatchType last_match_type;
  GURL last_suggested_url;
  bool last_is_lookalike = IsLookalikeUrl(
      last_url, engaged_sites, &last_match_type, &last_suggested_url);

  // If the first URL is a lookalike, but we ended up on the suggested site
  // anyway, don't warn.
  if (first_is_lookalike &&
      last_url.DomainIs(GetETLDPlusOne(first_suggested_url.host()))) {
    first_is_lookalike = false;
  }

  // Allow signed exchange cache URLs such as
  // https://example-com.site.test/package.sxg.
  // Navigation throttles see signed exchanges as a redirect chain where
  // Url 0: Cache URL (i.e. outer URL)
  // Url 1: URL of the sgx package
  // Url 2: Inner URL (the URL whose contents the sgx package contains)
  //
  // We want to allow lookalike cache URLs but not lookalike inner URLs, so we
  // make an exception for this condition.
  // TODO(meacer): Confirm that the assumption about cache URL being the 1st
  // and inner URL being the last URL in the redirect chain is correct.
  //
  // Note that the signed exchange logic can still redirect the initial
  // navigation to the fallback URL even if SGX checks fail (invalid cert,
  // missing headers etc, see crbug.com/874323 for an example). Such navigations
  // are not considered SGX navigations and IsSignedExchangeInnerResponse()
  // will return false. We treat such navigations as simple redirects.
  if (first_is_lookalike &&
      navigation_handle()->IsSignedExchangeInnerResponse()) {
    first_is_lookalike = false;
  }

  if (!first_is_lookalike && !last_is_lookalike) {
    return NavigationThrottle::PROCEED;
  }
  // IMPORTANT: Do not modify first_is_lookalike or last_is_lookalike beyond
  // this line. See crbug.com/1138138 for an example bug.

  // source_id corresponds to last_url, even when first_url is what triggered.
  // UKM records first_is_lookalike/triggered_by_initial_url to disambiguate.
  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle()->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);

  if (first_is_lookalike &&
      ShouldBlockLookalikeUrlNavigation(first_match_type)) {
    return CheckManifestsAndMaybeShowInterstitial(
        first_suggested_url, first_url, source_id, first_match_type,
        first_is_lookalike);
  }

  if (last_is_lookalike && ShouldBlockLookalikeUrlNavigation(last_match_type)) {
    return CheckManifestsAndMaybeShowInterstitial(last_suggested_url, last_url,
                                                  source_id, last_match_type,
                                                  first_is_lookalike);
  }

  LookalikeUrlMatchType match_type =
      first_is_lookalike ? first_match_type : last_match_type;
  if (match_type == LookalikeUrlMatchType::kCharacterSwapSiteEngagement ||
      match_type == LookalikeUrlMatchType::kCharacterSwapTop500) {
    GURL lookalike_url = first_is_lookalike ? first_url : last_url;

    navigation_handle()->GetRenderFrameHost()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(
            "Chrome has determined that %s could be fake or fraudulent.\n\n"
            "Future Chrome versions will show a warning on this domain name. "
            "If you believe this is shown in error please visit "
            "https://g.co/chrome/lookalike-warnings",
            lookalike_url.host().c_str()));
  }

  RecordUMAFromMatchType(match_type);
  // Interstitial normally records UKM, but still record when it's not shown.
  RecordUkmForLookalikeUrlBlockingPage(
      source_id, match_type,
      LookalikeUrlBlockingPageUserAction::kInterstitialNotShown,
      first_is_lookalike);
  return NavigationThrottle::PROCEED;
}

bool LookalikeUrlNavigationThrottle::IsLookalikeUrl(
    const GURL& url,
    const std::vector<DomainInfo>& engaged_sites,
    LookalikeUrlMatchType* match_type,
    GURL* suggested_url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Don't warn on non-public domains.
  if (net::HostStringIsLocalhost(url.host()) ||
      net::IsHostnameNonUnique(url.host()) ||
      GetETLDPlusOne(url.host()).empty()) {
    return false;
  }

  // Fetch the component allowlist.
  const auto* proto = reputation::GetSafetyTipsRemoteConfigProto();

  // When there's no proto (like at browser start), fail-safe and don't block.
  if (!proto) {
    return false;
  }

  // If the URL is in the local temporary allowlist, don't show any warning.
  if (ReputationService::Get(profile_)->IsIgnored(url)) {
    return false;
  }

  // If the host is allowlisted by policy, don't show any warning.
  if (IsAllowedByEnterprisePolicy(profile_->GetPrefs(), url)) {
    return false;
  }

  // If the URL is in the component allowlist, don't show any warning.
  if (reputation::IsUrlAllowlistedBySafetyTipsComponent(
          proto, url.GetWithEmptyPath())) {
    return false;
  }

  // GetDomainInfo() is expensive, so do possible early-abort checks first.
  const DomainInfo navigated_domain = GetDomainInfo(url);
  if (IsTopDomain(navigated_domain)) {
    return false;
  }

  // Ensure that this URL is not already engaged. We can't use the synchronous
  // SiteEngagementService::IsEngagementAtLeast as it has side effects. We check
  // in PerformChecks to ensure we have up-to-date engaged_sites. This check
  // ignores the scheme which is okay since it's more conservative: If the user
  // is engaged with http://domain.test, not showing the warning on
  // https://domain.test is acceptable.
  const auto already_engaged =
      std::find_if(engaged_sites.begin(), engaged_sites.end(),
                   [navigated_domain](const DomainInfo& engaged_domain) {
                     return (navigated_domain.domain_and_registry ==
                             engaged_domain.domain_and_registry);
                   });
  if (already_engaged != engaged_sites.end()) {
    return false;
  }

  const LookalikeTargetAllowlistChecker in_target_allowlist =
      base::BindRepeating(
          &reputation::IsTargetHostAllowlistedBySafetyTipsComponent, proto);
  std::string matched_domain;
  if (GetMatchingDomain(navigated_domain, engaged_sites, in_target_allowlist,
                        proto, &matched_domain, match_type)) {
    DCHECK(!matched_domain.empty());

    // matched_domain can be a top domain or an engaged domain. Simply use its
    // eTLD+1 as the suggested domain.
    // 1. If matched_domain is a top domain: Top domain list already contains
    // eTLD+1s only so this works well.
    // 2. If matched_domain is an engaged domain and is not an eTLD+1, don't
    // suggest it. Otherwise, navigating to googlé.com and having engaged with
    // docs.google.com would suggest docs.google.com.
    //
    // When the navigated and matched domains are not eTLD+1s (e.g.
    // docs.googlé.com and docs.google.com), this will suggest google.com
    // instead of docs.google.com. This is less than ideal, but has two
    // benefits:
    // - Simpler code
    // - Fewer suggestions to non-existent domains. E.g. When the navigated
    // domain is nonexistent.googlé.com and the matched domain is
    // docs.google.com, we will suggest google.com instead of
    // nonexistent.google.com.
    std::string suggested_domain = GetETLDPlusOne(matched_domain);
    DCHECK(!suggested_domain.empty());
    // Drop everything but the parts of the origin.
    GURL::Replacements replace_host;
    replace_host.SetHostStr(suggested_domain);
    *suggested_url = url.ReplaceComponents(replace_host).GetWithEmptyPath();
    return true;
  }

  if (ShouldBlockBySpoofCheckResult(navigated_domain)) {
    *match_type = LookalikeUrlMatchType::kFailedSpoofChecks;
    *suggested_url = GURL();
    return true;
  }

  return false;
}
