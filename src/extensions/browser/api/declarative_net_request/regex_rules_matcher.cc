// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/regex_rules_matcher.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/request_params.h"
#include "extensions/browser/api/declarative_net_request/utils.h"

namespace extensions {
namespace declarative_net_request {
namespace flat_rule = url_pattern_index::flat;

namespace {

bool IsExtraHeadersMatcherInternal(
    const RegexRulesMatcher::RegexRulesList* regex_list) {
  DCHECK(regex_list);

  // We only support removing a subset of extra headers currently. If that
  // changes, the implementation here should change as well.
  static_assert(flat::ActionType_count == 6,
                "Modify this method to ensure IsExtraHeadersMatcherInternal is "
                "updated as new actions are added.");

  return std::any_of(regex_list->begin(), regex_list->end(),
                     [](const flat::RegexRule* regex_rule) {
                       return regex_rule->action_type() ==
                              flat::ActionType_modify_headers;
                     });
}

re2::StringPiece ToRE2StringPiece(const ::flatbuffers::String& str) {
  return re2::StringPiece(str.c_str(), str.size());
}

// Helper to check if the |rule| metadata matches the given request |params|.
bool DoesRuleMetadataMatchRequest(const flat_rule::UrlRule& rule,
                                  const RequestParams& params) {
  // Compares |element_type| and |is_third_party|.
  if (!url_pattern_index::DoesRuleFlagsMatch(rule, params.element_type,
                                             flat_rule::ActivationType_NONE,
                                             params.is_third_party)) {
    return false;
  }

  // Compares included and excluded domains.
  return url_pattern_index::DoesOriginMatchDomainList(
      params.first_party_origin, rule, false /* disable_generic_rules */);
}

bool IsBeforeRequestAction(flat::ActionType action_type) {
  switch (action_type) {
    case flat::ActionType_block:
    case flat::ActionType_allow:
    case flat::ActionType_redirect:
    case flat::ActionType_upgrade_scheme:
    case flat::ActionType_allow_all_requests:
      return true;
    case flat::ActionType_modify_headers:
      return false;
    case flat::ActionType_count:
      NOTREACHED();
  }
  return false;
}

}  // namespace

RegexRuleInfo::RegexRuleInfo(const flat::RegexRule* regex_rule,
                             const re2::RE2* regex)
    : regex_rule(regex_rule), regex(regex) {
  DCHECK(regex_rule);
  DCHECK(regex);
}
RegexRuleInfo::RegexRuleInfo(const RegexRuleInfo& info) = default;
RegexRuleInfo& RegexRuleInfo::operator=(const RegexRuleInfo& info) = default;

RegexRulesMatcher::RegexRulesMatcher(const ExtensionId& extension_id,
                                     RulesetID ruleset_id,
                                     const RegexRulesList* regex_list,
                                     const ExtensionMetadataList* metadata_list)
    : RulesetMatcherBase(extension_id, ruleset_id),
      regex_list_(regex_list),
      metadata_list_(metadata_list),
      is_extra_headers_matcher_(IsExtraHeadersMatcherInternal(regex_list)) {
  InitializeMatcher();
}

RegexRulesMatcher::~RegexRulesMatcher() = default;

std::vector<RequestAction> RegexRulesMatcher::GetModifyHeadersActions(
    const RequestParams& params) const {
  const std::vector<RegexRuleInfo>& potential_matches =
      GetPotentialMatches(params);

  std::vector<const flat_rule::UrlRule*> rules;
  for (const RegexRuleInfo& info : potential_matches) {
    if (info.regex_rule->action_type() == flat::ActionType_modify_headers &&
        re2::RE2::PartialMatch(params.url->spec(), *info.regex)) {
      rules.push_back(info.regex_rule->url_rule());
    }
  }

  return GetModifyHeadersActionsFromMetadata(params, rules, *metadata_list_);
}

base::Optional<RequestAction> RegexRulesMatcher::GetAllowAllRequestsAction(
    const RequestParams& params) const {
  const std::vector<RegexRuleInfo>& potential_matches =
      GetPotentialMatches(params);
  auto info = std::find_if(potential_matches.begin(), potential_matches.end(),
                           [&params](const RegexRuleInfo& info) {
                             return info.regex_rule->action_type() ==
                                        flat::ActionType_allow_all_requests &&
                                    re2::RE2::PartialMatch(params.url->spec(),
                                                           *info.regex);
                           });
  if (info == potential_matches.end())
    return base::nullopt;

  return CreateAllowAllRequestsAction(params, *info->regex_rule->url_rule());
}

base::Optional<RequestAction>
RegexRulesMatcher::GetBeforeRequestActionIgnoringAncestors(
    const RequestParams& params) const {
  const std::vector<RegexRuleInfo>& potential_matches =
      GetPotentialMatches(params);
  auto info = std::find_if(
      potential_matches.begin(), potential_matches.end(),
      [&params](const RegexRuleInfo& info) {
        return IsBeforeRequestAction(info.regex_rule->action_type()) &&
               re2::RE2::PartialMatch(params.url->spec(), *info.regex);
      });
  if (info == potential_matches.end())
    return base::nullopt;

  const flat_rule::UrlRule& rule = *info->regex_rule->url_rule();
  switch (info->regex_rule->action_type()) {
    case flat::ActionType_block:
      return CreateBlockOrCollapseRequestAction(params, rule);
    case flat::ActionType_allow:
      return CreateAllowAction(params, rule);
    case flat::ActionType_redirect:
      // If this is a regex substitution rule, handle the substitution. Else
      // create the redirect action from the information in |metadata_list_|
      // below.
      return info->regex_rule->regex_substitution()
                 ? CreateRegexSubstitutionRedirectAction(params, *info)
                 : CreateRedirectActionFromMetadata(params, rule,
                                                    *metadata_list_);
    case flat::ActionType_upgrade_scheme:
      return CreateUpgradeAction(params, rule);
    case flat::ActionType_allow_all_requests:
      return CreateAllowAllRequestsAction(params, rule);
    case flat::ActionType_modify_headers:
    case flat::ActionType_count:
      NOTREACHED();
      break;
  }

  return base::nullopt;
}

void RegexRulesMatcher::InitializeMatcher() {
  if (IsEmpty())
    return;

  for (const auto* regex_rule : *regex_list_) {
    const flat_rule::UrlRule* rule = regex_rule->url_rule();

    const bool is_case_sensitive =
        !(rule->options() & flat_rule::OptionFlag_IS_CASE_INSENSITIVE);

    const bool require_capturing = !!regex_rule->regex_substitution();

    // TODO(karandeepb): Regex compilation can be expensive and sometimes we are
    // compiling the same regex twice, once during rule indexing and now during
    // ruleset loading. We should try maintaining a global cache of compiled
    // regexes and modify FilteredRE2 to take a regex object directly.
    int re2_id;
    re2::RE2::ErrorCode error_code = filtered_re2_.Add(
        ToRE2StringPiece(*rule->url_pattern()),
        CreateRE2Options(is_case_sensitive, require_capturing), &re2_id);

    // Ideally there shouldn't be any error, since we had already validated the
    // regular expression while indexing the ruleset. That said, there are cases
    // possible where this may happen, for example, the library's implementation
    // may change etc.
    // TODO(crbug.com/1050780): Notify the extension about the same.
    if (error_code != re2::RE2::NoError)
      continue;

    const bool did_insert =
        re2_id_to_rules_map_.insert({re2_id, regex_rule}).second;
    DCHECK(did_insert) << "Duplicate |re2_id| seen.";
  }

  // FilteredRE2 on compilation yields a set of candidate strings. These aid in
  // pre-filtering and obtaining the set of potential matches for a request.
  std::vector<std::string> strings_to_match;
  filtered_re2_.Compile(&strings_to_match);

  // FilteredRE2 guarantees that the returned set of candidate strings is
  // lower-cased.
  DCHECK(std::all_of(strings_to_match.begin(), strings_to_match.end(),
                     [](const std::string& s) {
                       return std::all_of(s.begin(), s.end(), [](const char c) {
                         return !base::IsAsciiUpper(c);
                       });
                     }));

  // Convert |strings_to_match| to StringPatterns. This is necessary to use
  // url_matcher::SubstringSetMatcher.
  std::vector<url_matcher::StringPattern> patterns;
  patterns.reserve(strings_to_match.size());

  for (size_t i = 0; i < strings_to_match.size(); ++i)
    patterns.emplace_back(std::move(strings_to_match[i]), i);

  substring_matcher_ =
      std::make_unique<url_matcher::SubstringSetMatcher>(patterns);
}

bool RegexRulesMatcher::IsEmpty() const {
  return regex_list_->Length() == 0;
}

const std::vector<RegexRuleInfo>& RegexRulesMatcher::GetPotentialMatches(
    const RequestParams& params) const {
  auto iter = params.potential_regex_matches.find(this);
  if (iter != params.potential_regex_matches.end())
    return iter->second;

  // Early out if this is an empty matcher.
  if (IsEmpty()) {
    auto result = params.potential_regex_matches.insert(
        std::make_pair(this, std::vector<RegexRuleInfo>()));
    return result.first->second;
  }

  // Compute the potential matches. FilteredRE2 requires the text to be lower
  // cased first.
  if (!params.lower_cased_url_spec)
    params.lower_cased_url_spec = base::ToLowerASCII(params.url->spec());

  // To pre-filter the set of regexes to match against |params|, we first need
  // to compute the set of candidate strings tracked by |substring_matcher_|
  // within |params.lower_cased_url_spec|.
  std::set<int> candidate_ids_set;
  DCHECK(substring_matcher_);
  substring_matcher_->Match(*params.lower_cased_url_spec, &candidate_ids_set);
  std::vector<int> candidate_ids_list(candidate_ids_set.begin(),
                                      candidate_ids_set.end());

  // FilteredRE2 then yields the set of potential regex matches.
  std::vector<int> potential_re2_ids;
  filtered_re2_.AllPotentials(candidate_ids_list, &potential_re2_ids);

  // We prune the set of potential matches even further by matching request
  // metadata.
  std::vector<RegexRuleInfo> potential_matches;
  for (int re2_id : potential_re2_ids) {
    auto it = re2_id_to_rules_map_.find(re2_id);
    DCHECK(it != re2_id_to_rules_map_.end());

    const flat::RegexRule* rule = it->second;
    if (!DoesRuleMetadataMatchRequest(*rule->url_rule(), params))
      continue;

    const RE2& regex = filtered_re2_.GetRE2(re2_id);
    potential_matches.emplace_back(rule, &regex);
  }

  // Sort potential matches in descending order of priority.
  std::sort(potential_matches.begin(), potential_matches.end(),
            [](const RegexRuleInfo& lhs, const RegexRuleInfo& rhs) {
              return lhs.regex_rule->url_rule()->priority() >
                     rhs.regex_rule->url_rule()->priority();
            });

  // Cache |potential_matches|.
  auto result = params.potential_regex_matches.insert(
      std::make_pair(this, std::move(potential_matches)));
  return result.first->second;
}

base::Optional<RequestAction>
RegexRulesMatcher::CreateRegexSubstitutionRedirectAction(
    const RequestParams& params,
    const RegexRuleInfo& info) const {
  // We could have extracted the captured strings during the matching stage
  // and directly used RE2::Rewrite here (which doesn't need to match the
  // regex again). However we prefer to capture the strings only when
  // necessary. Not capturing the strings should allow re2 to perform
  // additional optimizations during the matching stage.
  std::string redirect_str = params.url->spec();
  bool success =
      RE2::Replace(&redirect_str, *info.regex,
                   ToRE2StringPiece(*info.regex_rule->regex_substitution()));
  if (!success) {
    // This should generally not happen since we had already checked for a
    // match and during indexing, had verified that the substitution pattern
    // is not ill-formed. However, the re2 library implementation might have
    // changed since indexing, causing this.
    LOG(ERROR) << base::StringPrintf(
        "Rewrite failed. Regex:%s Substitution:%s URL:%s\n",
        info.regex->pattern().c_str(),
        info.regex_rule->regex_substitution()->c_str(),
        params.url->spec().c_str());
    return base::nullopt;
  }

  GURL redirect_url(redirect_str);

  // Redirects to JavaScript urls are not allowed.
  // TODO(crbug.com/1033780): this results in counterintuitive behavior.
  if (redirect_url.SchemeIs(url::kJavaScriptScheme))
    return base::nullopt;

  return CreateRedirectAction(params, *info.regex_rule->url_rule(),
                              std::move(redirect_url));
}

}  // namespace declarative_net_request
}  // namespace extensions
