// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/extension_url_pattern_index_matcher.h"

#include <algorithm>
#include <limits>
#include <list>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/common/api/declarative_net_request.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = api::declarative_net_request;

namespace {

using FindRuleStrategy =
    url_pattern_index::UrlPatternIndexMatcher::FindRuleStrategy;

std::vector<url_pattern_index::UrlPatternIndexMatcher> GetMatchers(
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList* index_list) {
  DCHECK(index_list);
  DCHECK_EQ(flat::IndexType_count, index_list->size());

  std::vector<url_pattern_index::UrlPatternIndexMatcher> matchers;
  matchers.reserve(flat::IndexType_count);
  for (const flat_rule::UrlPatternIndex* index : *index_list)
    matchers.emplace_back(index);
  return matchers;
}

bool IsExtraHeadersMatcherInternal(
    const std::vector<url_pattern_index::UrlPatternIndexMatcher>& matchers) {
  static_assert(flat::IndexType_count == 3,
                "Modify this method to ensure IsExtraHeadersMatcherInternal is "
                "updated as new actions are added.");
  return matchers[flat::IndexType_modify_headers].GetRulesCount() > 0;
}

size_t GetRulesCountInternal(
    const std::vector<url_pattern_index::UrlPatternIndexMatcher>& matchers) {
  size_t rules_count = 0;
  for (const auto& matcher : matchers)
    rules_count += matcher.GetRulesCount();

  return rules_count;
}

}  // namespace

ExtensionUrlPatternIndexMatcher::ExtensionUrlPatternIndexMatcher(
    const ExtensionId& extension_id,
    RulesetID ruleset_id,
    const ExtensionUrlPatternIndexMatcher::UrlPatternIndexList* index_list,
    const ExtensionMetadataList* metadata_list)
    : RulesetMatcherBase(extension_id, ruleset_id),
      metadata_list_(metadata_list),
      matchers_(GetMatchers(index_list)),
      is_extra_headers_matcher_(IsExtraHeadersMatcherInternal(matchers_)),
      rules_count_(GetRulesCountInternal(matchers_)) {}

ExtensionUrlPatternIndexMatcher::~ExtensionUrlPatternIndexMatcher() = default;

base::Optional<RequestAction>
ExtensionUrlPatternIndexMatcher::GetAllowAllRequestsAction(
    const RequestParams& params) const {
  const flat_rule::UrlRule* rule =
      GetMatchingRule(params, flat::IndexType_allow_all_requests,
                      FindRuleStrategy::kHighestPriority);
  if (!rule)
    return base::nullopt;

  return CreateAllowAllRequestsAction(params, *rule);
}

std::vector<RequestAction>
ExtensionUrlPatternIndexMatcher::GetModifyHeadersActions(
    const RequestParams& params) const {
  std::vector<const flat_rule::UrlRule*> rules =
      GetAllMatchingRules(params, flat::IndexType_modify_headers);

  return GetModifyHeadersActionsFromMetadata(params, rules, *metadata_list_);
}

base::Optional<RequestAction>
ExtensionUrlPatternIndexMatcher::GetBeforeRequestActionIgnoringAncestors(
    const RequestParams& params) const {
  return GetMaxPriorityAction(GetBeforeRequestActionHelper(params),
                              GetAllowAllRequestsAction(params));
}

base::Optional<RequestAction>
ExtensionUrlPatternIndexMatcher::GetBeforeRequestActionHelper(
    const RequestParams& params) const {
  const flat_rule::UrlRule* rule = GetMatchingRule(
      params, flat::IndexType_before_request_except_allow_all_requests,
      FindRuleStrategy::kHighestPriority);
  if (!rule)
    return base::nullopt;

  const flat::UrlRuleMetadata* metadata =
      metadata_list_->LookupByKey(rule->id());
  DCHECK(metadata);
  DCHECK_EQ(metadata->id(), rule->id());
  switch (metadata->action()) {
    case flat::ActionType_block:
      return CreateBlockOrCollapseRequestAction(params, *rule);
    case flat::ActionType_allow:
      return CreateAllowAction(params, *rule);
    case flat::ActionType_redirect:
      return CreateRedirectActionFromMetadata(params, *rule, *metadata_list_);
    case flat::ActionType_upgrade_scheme:
      return CreateUpgradeAction(params, *rule);
    case flat::ActionType_allow_all_requests:
    case flat::ActionType_modify_headers:
    case flat::ActionType_count:
      NOTREACHED();
  }

  return base::nullopt;
}

const flat_rule::UrlRule* ExtensionUrlPatternIndexMatcher::GetMatchingRule(
    const RequestParams& params,
    flat::IndexType index,
    FindRuleStrategy strategy) const {
  DCHECK_LT(index, flat::IndexType_count);
  DCHECK_GE(index, 0);
  DCHECK(params.url);

  // Don't exclude generic rules from being matched. A generic rule is one with
  // an empty included domains list.
  const bool kDisableGenericRules = false;

  return matchers_[index].FindMatch(
      *params.url, params.first_party_origin, params.element_type,
      flat_rule::ActivationType_NONE, params.is_third_party,
      kDisableGenericRules, strategy);
}

std::vector<const url_pattern_index::flat::UrlRule*>
ExtensionUrlPatternIndexMatcher::GetAllMatchingRules(
    const RequestParams& params,
    flat::IndexType index) const {
  DCHECK_LT(index, flat::IndexType_count);
  DCHECK_GE(index, 0);
  DCHECK(params.url);

  // Don't exclude generic rules from being matched. A generic rule is one with
  // an empty included domains list.
  const bool kDisableGenericRules = false;

  return matchers_[index].FindAllMatches(
      *params.url, params.first_party_origin, params.element_type,
      flat_rule::ActivationType_NONE, params.is_third_party,
      kDisableGenericRules);
}

}  // namespace declarative_net_request
}  // namespace extensions
