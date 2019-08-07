// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "extensions/common/constants.h"
#include "net/http/http_request_headers.h"
#include "url/origin.h"

namespace extensions {
namespace declarative_net_request {
namespace {

namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;
using PageAccess = PermissionsData::PageAccess;

// Describes the different cases pertaining to initiator checks to find the main
// frame url for a main frame subresource.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PageAllowingInitiatorCheck {
  kInitiatorAbsent = 0,
  kNeitherCandidateMatchesInitiator = 1,
  kCommittedCandidateMatchesInitiator = 2,
  kPendingCandidateMatchesInitiator = 3,
  kBothCandidatesMatchInitiator = 4,
  kMaxValue = kBothCandidatesMatchInitiator,
};

constexpr const char kSetCookieResponseHeader[] = "set-cookie";

// Returns true if |request| came from a page from the set of
// |allowed_pages|. This necessitates finding the main frame url
// corresponding to |request|. The logic behind how this is done is subtle and
// as follows:
//   - Requests made by the browser (not including navigation/frame requests) or
//     service worker: These requests don't correspond to a render frame and
//     hence they are not considered for allowing using the page
//     allowing API.
//   - Requests that correspond to a page: These include:
//     - Main frame request: To check if it is allowed, check the request
//       url against the set of allowed pages.
//     - Main frame subresource request: We might not be able to
//       deterministically map a main frame subresource to the main frame url.
//       This is because when a main frame subresource request reaches the
//       browser, the main frame navigation would have been committed in the
//       renderer, but the browser may not have been notified of the commit.
//       Hence the FrameData for the request may not have the correct value for
//       the |last_committed_main_frame_url|. To get around this we use
//       FrameData's |pending_main_frame_url| which is populated in
//       WebContentsObserver::ReadyToCommitNavigation. This happens before the
//       renderer is asked to commit the navigation.
//     - Subframe subresources: When a subframe subresource request reaches the
//       browser, it is assured that the browser knows about its parent frame
//       commit. For these requests, use the |last_committed_main_frame_url| and
//       match it against the set of allowed pages.
bool IsRequestPageAllowed(const WebRequestInfo& request,
                          const URLPatternSet& allowed_pages) {
  if (allowed_pages.is_empty())
    return false;

  // If this is a main frame request, |request.url| will be the main frame url.
  if (request.type == content::ResourceType::kMainFrame)
    return allowed_pages.MatchesURL(request.url);

  // This should happen for:
  //  - Requests not corresponding to a render frame e.g. non-navigation
  //    browser requests or service worker requests.
  //  - Requests made by a render frame but when we don't have cached FrameData
  //    for the request. This should occur rarely and is tracked by the
  //    "Extensions.ExtensionFrameMapCacheHit" histogram
  if (!request.frame_data)
    return false;

  const bool evaluate_pending_main_frame_url =
      request.frame_data->pending_main_frame_url &&
      *request.frame_data->pending_main_frame_url !=
          request.frame_data->last_committed_main_frame_url;

  if (!evaluate_pending_main_frame_url) {
    return allowed_pages.MatchesURL(
        request.frame_data->last_committed_main_frame_url);
  }

  // |pending_main_frame_url| should only be set for main-frame subresource
  // loads.
  DCHECK_EQ(ExtensionApiFrameIdMap::kTopFrameId, request.frame_data->frame_id);

  auto log_uma = [](PageAllowingInitiatorCheck value) {
    UMA_HISTOGRAM_ENUMERATION(
        "Extensions.DeclarativeNetRequest.PageWhitelistingInitiatorCheck",
        value);
  };

  // At this point, we are evaluating a main-frame subresource. There are two
  // candidate main frame urls - |pending_main_frame_url| and
  // |last_committed_main_frame_url|. To predict the correct main frame url,
  // compare the request initiator (origin of the requesting frame i.e. origin
  // of the main frame in this case) with the candidate urls' origins. If only
  // one of the candidate url's origin matches the request initiator, we can be
  // reasonably sure that it is the correct main frame url.
  if (!request.initiator) {
    log_uma(PageAllowingInitiatorCheck::kInitiatorAbsent);
  } else {
    const bool initiator_matches_pending_url =
        url::Origin::Create(*request.frame_data->pending_main_frame_url) ==
        *request.initiator;
    const bool initiator_matches_committed_url =
        url::Origin::Create(
            request.frame_data->last_committed_main_frame_url) ==
        *request.initiator;

    if (initiator_matches_pending_url && !initiator_matches_committed_url) {
      // We predict that |pending_main_frame_url| is the actual main frame url.
      log_uma(PageAllowingInitiatorCheck::kPendingCandidateMatchesInitiator);
      return allowed_pages.MatchesURL(
          *request.frame_data->pending_main_frame_url);
    }

    if (initiator_matches_committed_url && !initiator_matches_pending_url) {
      // We predict that |last_committed_main_frame_url| is the actual main
      // frame url.
      log_uma(PageAllowingInitiatorCheck::kCommittedCandidateMatchesInitiator);
      return allowed_pages.MatchesURL(
          request.frame_data->last_committed_main_frame_url);
    }

    if (initiator_matches_pending_url && initiator_matches_committed_url) {
      log_uma(PageAllowingInitiatorCheck::kBothCandidatesMatchInitiator);
    } else {
      DCHECK(!initiator_matches_pending_url);
      DCHECK(!initiator_matches_committed_url);
      log_uma(PageAllowingInitiatorCheck::kNeitherCandidateMatchesInitiator);
    }
  }

  // If we are not able to correctly predict the main frame url, simply test
  // against both the possible URLs. This means a small proportion of main frame
  // subresource requests might be incorrectly allowed by the page
  // allowing API.
  return allowed_pages.MatchesURL(
             request.frame_data->last_committed_main_frame_url) ||
         allowed_pages.MatchesURL(*request.frame_data->pending_main_frame_url);
}

bool ShouldCollapseResourceType(flat_rule::ElementType type) {
  // TODO(crbug.com/848842): Add support for other element types like
  // OBJECT.
  return type == flat_rule::ElementType_IMAGE ||
         type == flat_rule::ElementType_SUBDOCUMENT;
}

void NotifyRequestWithheld(const ExtensionId& extension_id,
                           const WebRequestInfo& request) {
  DCHECK(ExtensionsAPIClient::Get());
  ExtensionsAPIClient::Get()->NotifyWebRequestWithheld(
      request.render_process_id, request.frame_id, extension_id);
}

// Populates the list of headers corresponding to |mask|.
void PopulateHeadersFromMask(uint8_t mask,
                             std::vector<const char*>* request_headers,
                             std::vector<const char*>* response_headers) {
  DCHECK(request_headers);
  DCHECK(response_headers);

  uint8_t bit = 0;
  // Iterate over each RemoveHeaderType value.
  for (int i = 0; mask && i <= dnr_api::REMOVE_HEADER_TYPE_LAST; ++i) {
    switch (i) {
      case dnr_api::REMOVE_HEADER_TYPE_NONE:
        break;
      case dnr_api::REMOVE_HEADER_TYPE_COOKIE:
        bit = kRemoveHeadersMask_Cookie;
        if (mask & bit) {
          mask &= ~bit;
          request_headers->push_back(net::HttpRequestHeaders::kCookie);
        }
        break;
      case dnr_api::REMOVE_HEADER_TYPE_REFERER:
        bit = kRemoveHeadersMask_Referer;
        if (mask & bit) {
          mask &= ~bit;
          request_headers->push_back(net::HttpRequestHeaders::kReferer);
        }
        break;
      case dnr_api::REMOVE_HEADER_TYPE_SETCOOKIE:
        bit = kRemoveHeadersMask_SetCookie;
        if (mask & bit) {
          mask &= ~bit;
          response_headers->push_back(kSetCookieResponseHeader);
        }
        break;
    }
  }
}

}  // namespace

RulesetManager::Action::Action(Action::Type type) : type(type) {}
RulesetManager::Action::~Action() = default;
RulesetManager::Action::Action(Action&&) = default;
RulesetManager::Action& RulesetManager::Action::operator=(Action&&) = default;

bool RulesetManager::Action::operator==(const Action& that) const {
  return type == that.type && redirect_url == that.redirect_url;
}

RulesetManager::RulesetManager(const InfoMap* info_map) : info_map_(info_map) {
  DCHECK(info_map_);

  // RulesetManager can be created on any sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RulesetManager::~RulesetManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RulesetManager::AddRuleset(const ExtensionId& extension_id,
                                std::unique_ptr<CompositeMatcher> matcher,
                                URLPatternSet allowed_pages) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAPIAvailable());

  bool inserted;
  std::tie(std::ignore, inserted) =
      rulesets_.emplace(extension_id, info_map_->GetInstallTime(extension_id),
                        std::move(matcher), std::move(allowed_pages));
  DCHECK(inserted) << "AddRuleset called twice in succession for "
                   << extension_id;

  if (test_observer_)
    test_observer_->OnRulesetCountChanged(rulesets_.size());

  // Clear the renderers' cache so that they take the new rules into account.
  ClearRendererCacheOnNavigation();
}

void RulesetManager::RemoveRuleset(const ExtensionId& extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAPIAvailable());

  auto compare_by_id =
      [&extension_id](const ExtensionRulesetData& ruleset_data) {
        return ruleset_data.extension_id == extension_id;
      };

  DCHECK(std::find_if(rulesets_.begin(), rulesets_.end(), compare_by_id) !=
         rulesets_.end())
      << "RemoveRuleset called without a corresponding AddRuleset for "
      << extension_id;

  base::EraseIf(rulesets_, compare_by_id);

  if (test_observer_)
    test_observer_->OnRulesetCountChanged(rulesets_.size());

  // Clear the renderers' cache so that they take the removed rules into
  // account.
  ClearRendererCacheOnNavigation();
}

CompositeMatcher* RulesetManager::GetMatcherForExtension(
    const ExtensionId& extension_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAPIAvailable());

  // This is O(n) but it's ok since the number of extensions will be small and
  // we have to maintain the rulesets sorted in decreasing order of installation
  // time.
  auto iter =
      std::find_if(rulesets_.begin(), rulesets_.end(),
                   [&extension_id](const ExtensionRulesetData& ruleset) {
                     return ruleset.extension_id == extension_id;
                   });

  // There must be ExtensionRulesetData corresponding to this |extension_id|.
  if (iter == rulesets_.end())
    return nullptr;

  DCHECK(iter->matcher);
  return iter->matcher.get();
}

void RulesetManager::UpdateAllowedPages(const ExtensionId& extension_id,
                                        URLPatternSet allowed_pages) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAPIAvailable());

  // This is O(n) but it's ok since the number of extensions will be small and
  // we have to maintain the rulesets sorted in decreasing order of installation
  // time.
  auto iter =
      std::find_if(rulesets_.begin(), rulesets_.end(),
                   [&extension_id](const ExtensionRulesetData& ruleset) {
                     return ruleset.extension_id == extension_id;
                   });

  // There must be ExtensionRulesetData corresponding to this |extension_id|.
  DCHECK(iter != rulesets_.end());

  iter->allowed_pages = std::move(allowed_pages);

  // Clear the renderers' cache so that they take the updated allowed pages
  // into account.
  ClearRendererCacheOnNavigation();
}

RulesetManager::Action RulesetManager::EvaluateRequest(
    const WebRequestInfo& request,
    bool is_incognito_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ShouldEvaluateRequest(request))
    return Action(Action::Type::NONE);

  if (test_observer_)
    test_observer_->OnEvaluateRequest(request, is_incognito_context);

  if (rulesets_.empty())
    return Action(Action::Type::NONE);

  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.DeclarativeNetRequest.EvaluateRequestTime.AllExtensions2");

  RequestParams params(request);
  const int tab_id = request.frame_data ? request.frame_data->tab_id
                                        : extension_misc::kUnknownTabId;

  // |crosses_incognito| is used to ensure that a split mode extension process
  // can't intercept requests from a cross browser context. Since declarative
  // net request API doesn't use event listeners in a background process, it is
  // irrelevant here.
  const bool crosses_incognito = false;

  std::vector<bool> should_evaluate_rulesets_for_request(rulesets_.size());

  // We first check if any extension wants the request to be blocked.
  {
    size_t i = 0;
    for (auto ruleset_data = rulesets_.begin(); ruleset_data != rulesets_.end();
         ++ruleset_data, ++i) {
      // As a minor optimization, cache the value of
      // |ShouldEvaluateRulesetForRequest|.
      should_evaluate_rulesets_for_request[i] = ShouldEvaluateRulesetForRequest(
          *ruleset_data, request, is_incognito_context);
      if (!should_evaluate_rulesets_for_request[i])
        continue;

      // Now check if the extension has access to the request. Note: the
      // extension does not require host permissions to block network requests.
      PageAccess page_access = WebRequestPermissions::CanExtensionAccessURL(
          info_map_, ruleset_data->extension_id, request.url, tab_id,
          crosses_incognito, WebRequestPermissions::DO_NOT_CHECK_HOST,
          request.initiator, request.type);
      DCHECK_NE(PageAccess::kWithheld, page_access);
      if (page_access != PageAccess::kAllowed)
        continue;

      if (ruleset_data->matcher->ShouldBlockRequest(params)) {
        return ShouldCollapseResourceType(params.element_type)
                   ? Action(Action::Type::COLLAPSE)
                   : Action(Action::Type::BLOCK);
      }
    }
  }

  // The request shouldn't be blocked. Now check if any extension wants to
  // redirect the request.

  // Redirecting WebSocket handshake request is prohibited.
  if (params.element_type == flat_rule::ElementType_WEBSOCKET)
    return Action(Action::Type::NONE);

  // This iterates in decreasing order of extension installation time. Hence
  // more recently installed extensions get higher priority in choosing the
  // redirect url.
  {
    size_t i = 0;
    for (auto ruleset_data = rulesets_.begin(); ruleset_data != rulesets_.end();
         ++ruleset_data, ++i) {
      if (!should_evaluate_rulesets_for_request[i])
        continue;

      // Redirecting a request requires host permissions to the request url and
      // its initiator.
      PageAccess page_access = WebRequestPermissions::CanExtensionAccessURL(
          info_map_, ruleset_data->extension_id, request.url, tab_id,
          crosses_incognito,
          WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
          request.initiator, request.type);

      if (page_access != PageAccess::kAllowed) {
        if (page_access == PageAccess::kWithheld)
          NotifyRequestWithheld(ruleset_data->extension_id, request);
        continue;
      }

      GURL redirect_url;
      if (ruleset_data->matcher->ShouldRedirectRequest(params, &redirect_url)) {
        Action action(Action::Type::REDIRECT);
        action.redirect_url = std::move(redirect_url);
        return action;
      }
    }
  }

  // Now check if extensions want to remove headers.
  {
    size_t i = 0;
    uint8_t mask = 0;
    for (auto ruleset_data = rulesets_.begin(); ruleset_data != rulesets_.end();
         ++ruleset_data, ++i) {
      if (!should_evaluate_rulesets_for_request[i])
        continue;

      // Now check if the extension has access to the request. Note: the
      // extension does not require host permissions to remove headers.
      PageAccess page_access = WebRequestPermissions::CanExtensionAccessURL(
          info_map_, ruleset_data->extension_id, request.url, tab_id,
          crosses_incognito, WebRequestPermissions::DO_NOT_CHECK_HOST,
          request.initiator, request.type);
      DCHECK_NE(PageAccess::kWithheld, page_access);
      if (page_access != PageAccess::kAllowed)
        continue;

      mask |= ruleset_data->matcher->GetRemoveHeadersMask(params, mask);
    }

    if (mask) {
      Action action(Action::Type::REMOVE_HEADERS);
      PopulateHeadersFromMask(mask, &action.request_headers_to_remove,
                              &action.response_headers_to_remove);
      return action;
    }
  }

  return Action(Action::Type::NONE);
}

bool RulesetManager::HasAnyExtraHeadersMatcher() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& ruleset : rulesets_) {
    if (ruleset.matcher->HasAnyExtraHeadersMatcher())
      return true;
  }

  return false;
}

bool RulesetManager::HasExtraHeadersMatcherForRequest(
    const WebRequestInfo& request,
    bool is_incognito_context) const {
  // TODO(karandeepb): This causes multiple EvaluateRequest calls for a single
  // network request. We should cache the value of Action so that we only ever
  // evaluate the rulesets once for a request.
  Action action = EvaluateRequest(request, is_incognito_context);

  // We only support removing a subset of extra headers currently. If that
  // changes, the implementation here should change as well.
  static_assert(flat::ActionIndex_count == 6,
                "Modify this method to ensure HasExtraHeadersMatcherForRequest "
                "is updated as new actions are added.");

  return action.type == Action::Type::REMOVE_HEADERS;
}

void RulesetManager::SetObserverForTest(TestObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  test_observer_ = observer;
}

RulesetManager::ExtensionRulesetData::ExtensionRulesetData(
    const ExtensionId& extension_id,
    const base::Time& extension_install_time,
    std::unique_ptr<CompositeMatcher> matcher,
    URLPatternSet allowed_pages)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      matcher(std::move(matcher)),
      allowed_pages(std::move(allowed_pages)) {}
RulesetManager::ExtensionRulesetData::~ExtensionRulesetData() = default;
RulesetManager::ExtensionRulesetData::ExtensionRulesetData(
    ExtensionRulesetData&& other) = default;
RulesetManager::ExtensionRulesetData& RulesetManager::ExtensionRulesetData::
operator=(ExtensionRulesetData&& other) = default;

bool RulesetManager::ExtensionRulesetData::operator<(
    const ExtensionRulesetData& other) const {
  // Sort based on *descending* installation time, using extension id to break
  // ties.
  return std::tie(extension_install_time, extension_id) >
         std::tie(other.extension_install_time, other.extension_id);
}

bool RulesetManager::ShouldEvaluateRequest(
    const WebRequestInfo& request) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ensure clients filter out sensitive requests.
  DCHECK(!WebRequestPermissions::HideRequest(info_map_, request));

  if (!IsAPIAvailable()) {
    DCHECK(rulesets_.empty());
    return false;
  }

  // Prevent extensions from modifying any resources on the chrome-extension
  // scheme. Practically, this has the effect of not allowing an extension to
  // modify its own resources (The extension wouldn't have the permission to
  // other extension origins anyway).
  if (request.url.SchemeIs(kExtensionScheme))
    return false;

  return true;
}

bool RulesetManager::ShouldEvaluateRulesetForRequest(
    const ExtensionRulesetData& ruleset,
    const WebRequestInfo& request,
    bool is_incognito_context) const {
  // Only extensions enabled in incognito should have access to requests in an
  // incognito context.
  if (is_incognito_context &&
      !info_map_->IsIncognitoEnabled(ruleset.extension_id)) {
    return false;
  }

  if (IsRequestPageAllowed(request, ruleset.allowed_pages))
    return false;

  return true;
}

}  // namespace declarative_net_request
}  // namespace extensions
