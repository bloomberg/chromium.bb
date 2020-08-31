// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/indexed_rule.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/utils.h"
#include "net/http/http_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace extensions {
namespace declarative_net_request {

namespace {

namespace flat_rule = url_pattern_index::flat;
namespace dnr_api = extensions::api::declarative_net_request;

constexpr char kAnchorCharacter = '|';
constexpr char kSeparatorCharacter = '^';
constexpr char kWildcardCharacter = '*';

// Returns true if bitmask |sub| is a subset of |super|.
constexpr bool IsSubset(unsigned sub, unsigned super) {
  return (super | sub) == super;
}

// Helper class to parse the url filter of a Declarative Net Request API rule.
class UrlFilterParser {
 public:
  // This sets the |url_pattern_type|, |anchor_left|, |anchor_right| and
  // |url_pattern| fields on the |indexed_rule_|.
  static void Parse(std::unique_ptr<std::string> url_filter,
                    IndexedRule* indexed_rule) {
    DCHECK(indexed_rule);
    UrlFilterParser(url_filter ? std::move(*url_filter) : std::string(),
                    indexed_rule)
        .ParseImpl();
  }

 private:
  UrlFilterParser(std::string url_filter, IndexedRule* indexed_rule)
      : url_filter_(std::move(url_filter)),
        url_filter_len_(url_filter_.length()),
        index_(0),
        indexed_rule_(indexed_rule) {}

  void ParseImpl() {
    ParseLeftAnchor();
    DCHECK_LE(index_, 2u);

    ParseFilterString();
    DCHECK(index_ == url_filter_len_ || index_ + 1 == url_filter_len_);

    ParseRightAnchor();
    DCHECK_EQ(url_filter_len_, index_);
  }

  void ParseLeftAnchor() {
    indexed_rule_->anchor_left = flat_rule::AnchorType_NONE;

    if (IsAtAnchor()) {
      ++index_;
      indexed_rule_->anchor_left = flat_rule::AnchorType_BOUNDARY;
      if (IsAtAnchor()) {
        ++index_;
        indexed_rule_->anchor_left = flat_rule::AnchorType_SUBDOMAIN;
      }
    }
  }

  void ParseFilterString() {
    indexed_rule_->url_pattern_type = flat_rule::UrlPatternType_SUBSTRING;
    size_t left_index = index_;
    while (index_ < url_filter_len_ && !IsAtRightAnchor()) {
      if (IsAtSeparatorOrWildcard())
        indexed_rule_->url_pattern_type = flat_rule::UrlPatternType_WILDCARDED;
      ++index_;
    }
    // Note: Empty url patterns are supported.
    indexed_rule_->url_pattern =
        url_filter_.substr(left_index, index_ - left_index);
  }

  void ParseRightAnchor() {
    indexed_rule_->anchor_right = flat_rule::AnchorType_NONE;
    if (IsAtRightAnchor()) {
      ++index_;
      indexed_rule_->anchor_right = flat_rule::AnchorType_BOUNDARY;
    }
  }

  bool IsAtSeparatorOrWildcard() const {
    return IsAtValidIndex() && (url_filter_[index_] == kSeparatorCharacter ||
                                url_filter_[index_] == kWildcardCharacter);
  }

  bool IsAtRightAnchor() const {
    return IsAtAnchor() && index_ > 0 && index_ + 1 == url_filter_len_;
  }

  bool IsAtValidIndex() const { return index_ < url_filter_len_; }

  bool IsAtAnchor() const {
    return IsAtValidIndex() && url_filter_[index_] == kAnchorCharacter;
  }

  const std::string url_filter_;
  const size_t url_filter_len_;
  size_t index_;
  IndexedRule* indexed_rule_;  // Must outlive this instance.

  DISALLOW_COPY_AND_ASSIGN(UrlFilterParser);
};

bool IsCaseSensitive(const dnr_api::Rule& parsed_rule) {
  // If case sensitivity is not explicitly specified, rules are considered case
  // sensitive by default.
  if (!parsed_rule.condition.is_url_filter_case_sensitive)
    return true;

  return *parsed_rule.condition.is_url_filter_case_sensitive;
}

// Returns a bitmask of flat_rule::OptionFlag corresponding to |parsed_rule|.
uint8_t GetOptionsMask(const dnr_api::Rule& parsed_rule) {
  uint8_t mask = flat_rule::OptionFlag_NONE;

  if (parsed_rule.action.type == dnr_api::RULE_ACTION_TYPE_ALLOW)
    mask |= flat_rule::OptionFlag_IS_WHITELIST;

  if (!IsCaseSensitive(parsed_rule))
    mask |= flat_rule::OptionFlag_IS_CASE_INSENSITIVE;

  switch (parsed_rule.condition.domain_type) {
    case dnr_api::DOMAIN_TYPE_FIRSTPARTY:
      mask |= flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY;
      break;
    case dnr_api::DOMAIN_TYPE_THIRDPARTY:
      mask |= flat_rule::OptionFlag_APPLIES_TO_THIRD_PARTY;
      break;
    case dnr_api::DOMAIN_TYPE_NONE:
      mask |= (flat_rule::OptionFlag_APPLIES_TO_FIRST_PARTY |
               flat_rule::OptionFlag_APPLIES_TO_THIRD_PARTY);
      break;
  }
  return mask;
}

uint8_t GetActivationTypes(const dnr_api::Rule& parsed_rule) {
  // Extensions don't use any activation types currently.
  return flat_rule::ActivationType_NONE;
}

flat_rule::ElementType GetElementType(dnr_api::ResourceType resource_type) {
  switch (resource_type) {
    case dnr_api::RESOURCE_TYPE_NONE:
      return flat_rule::ElementType_NONE;
    case dnr_api::RESOURCE_TYPE_MAIN_FRAME:
      return flat_rule::ElementType_MAIN_FRAME;
    case dnr_api::RESOURCE_TYPE_SUB_FRAME:
      return flat_rule::ElementType_SUBDOCUMENT;
    case dnr_api::RESOURCE_TYPE_STYLESHEET:
      return flat_rule::ElementType_STYLESHEET;
    case dnr_api::RESOURCE_TYPE_SCRIPT:
      return flat_rule::ElementType_SCRIPT;
    case dnr_api::RESOURCE_TYPE_IMAGE:
      return flat_rule::ElementType_IMAGE;
    case dnr_api::RESOURCE_TYPE_FONT:
      return flat_rule::ElementType_FONT;
    case dnr_api::RESOURCE_TYPE_OBJECT:
      return flat_rule::ElementType_OBJECT;
    case dnr_api::RESOURCE_TYPE_XMLHTTPREQUEST:
      return flat_rule::ElementType_XMLHTTPREQUEST;
    case dnr_api::RESOURCE_TYPE_PING:
      return flat_rule::ElementType_PING;
    case dnr_api::RESOURCE_TYPE_CSP_REPORT:
      return flat_rule::ElementType_CSP_REPORT;
    case dnr_api::RESOURCE_TYPE_MEDIA:
      return flat_rule::ElementType_MEDIA;
    case dnr_api::RESOURCE_TYPE_WEBSOCKET:
      return flat_rule::ElementType_WEBSOCKET;
    case dnr_api::RESOURCE_TYPE_OTHER:
      return flat_rule::ElementType_OTHER;
  }
  NOTREACHED();
  return flat_rule::ElementType_NONE;
}

// Returns a bitmask of flat_rule::ElementType corresponding to passed
// |resource_types|.
uint16_t GetResourceTypesMask(
    const std::vector<dnr_api::ResourceType>* resource_types) {
  uint16_t mask = flat_rule::ElementType_NONE;
  if (!resource_types)
    return mask;

  for (const auto resource_type : *resource_types)
    mask |= GetElementType(resource_type);
  return mask;
}

// Computes the bitmask of flat_rule::ElementType taking into consideration the
// included and excluded resource types for |rule| and its associated action
// type.
ParseResult ComputeElementTypes(const dnr_api::Rule& rule,
                                uint16_t* element_types) {
  uint16_t include_element_type_mask =
      GetResourceTypesMask(rule.condition.resource_types.get());
  uint16_t exclude_element_type_mask =
      GetResourceTypesMask(rule.condition.excluded_resource_types.get());

  // OBJECT_SUBREQUEST is not used by Extensions.
  if (exclude_element_type_mask ==
      (flat_rule::ElementType_ANY &
       ~flat_rule::ElementType_OBJECT_SUBREQUEST)) {
    return ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES;
  }

  if (include_element_type_mask & exclude_element_type_mask)
    return ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED;

  if (rule.action.type == dnr_api::RULE_ACTION_TYPE_ALLOWALLREQUESTS) {
    // For allowAllRequests rule, the resourceTypes key must always be specified
    // and may only include main_frame and sub_frame types.
    const uint16_t frame_element_type_mask =
        flat_rule::ElementType_MAIN_FRAME | flat_rule::ElementType_SUBDOCUMENT;
    if (include_element_type_mask == flat_rule::ElementType_NONE ||
        !IsSubset(include_element_type_mask, frame_element_type_mask)) {
      return ParseResult::ERROR_INVALID_ALLOW_ALL_REQUESTS_RESOURCE_TYPE;
    }
  }

  if (include_element_type_mask != flat_rule::ElementType_NONE)
    *element_types = include_element_type_mask;
  else if (exclude_element_type_mask != flat_rule::ElementType_NONE)
    *element_types = flat_rule::ElementType_ANY & ~exclude_element_type_mask;
  else
    *element_types = url_pattern_index::kDefaultFlatElementTypesMask;

  return ParseResult::SUCCESS;
}

// Lower-cases and sorts |domains|, as required by the url_pattern_index
// component and stores the result in |output|. Returns false in case of
// failure, when one of the input strings contains non-ascii characters.
bool CanonicalizeDomains(std::unique_ptr<std::vector<std::string>> domains,
                         std::vector<std::string>* output) {
  DCHECK(output);
  DCHECK(output->empty());

  if (!domains)
    return true;

  // Convert to lower case as required by the url_pattern_index component.
  for (const std::string& domain : *domains) {
    if (!base::IsStringASCII(domain))
      return false;

    output->push_back(base::ToLowerASCII(domain));
  }

  std::sort(output->begin(), output->end(),
            [](const std::string& left, const std::string& right) {
              return url_pattern_index::CompareDomains(left, right) < 0;
            });

  return true;
}

// Returns if the redirect URL will be used as a relative URL.
bool IsRedirectUrlRelative(const std::string& redirect_url) {
  return !redirect_url.empty() && redirect_url[0] == '/';
}

bool IsValidTransformScheme(const std::unique_ptr<std::string>& scheme) {
  if (!scheme)
    return true;

  for (size_t i = 0; i < base::size(kAllowedTransformSchemes); ++i) {
    if (*scheme == kAllowedTransformSchemes[i])
      return true;
  }
  return false;
}

bool IsValidPort(const std::unique_ptr<std::string>& port) {
  if (!port || port->empty())
    return true;

  unsigned port_num = 0;
  return base::StringToUint(*port, &port_num) && port_num <= 65535;
}

bool IsEmptyOrStartsWith(const std::unique_ptr<std::string>& str,
                         char starts_with) {
  return !str || str->empty() || str->at(0) == starts_with;
}

// Validates the given url |transform|.
ParseResult ValidateTransform(const dnr_api::URLTransform& transform) {
  if (!IsValidTransformScheme(transform.scheme))
    return ParseResult::ERROR_INVALID_TRANSFORM_SCHEME;

  if (!IsValidPort(transform.port))
    return ParseResult::ERROR_INVALID_TRANSFORM_PORT;

  if (!IsEmptyOrStartsWith(transform.query, '?'))
    return ParseResult::ERROR_INVALID_TRANSFORM_QUERY;

  if (!IsEmptyOrStartsWith(transform.fragment, '#'))
    return ParseResult::ERROR_INVALID_TRANSFORM_FRAGMENT;

  // Only one of |query| or |query_transform| should be specified.
  if (transform.query && transform.query_transform)
    return ParseResult::ERROR_QUERY_AND_TRANSFORM_BOTH_SPECIFIED;

  return ParseResult::SUCCESS;
}

// Parses the "action.redirect" dictionary of a dnr_api::Rule.
ParseResult ParseRedirect(dnr_api::Redirect redirect,
                          const GURL& base_url,
                          IndexedRule* indexed_rule) {
  DCHECK(indexed_rule);

  if (redirect.url) {
    GURL redirect_url = GURL(*redirect.url);
    if (!redirect_url.is_valid())
      return ParseResult::ERROR_INVALID_REDIRECT_URL;

    if (redirect_url.SchemeIs(url::kJavaScriptScheme))
      return ParseResult::ERROR_JAVASCRIPT_REDIRECT;

    indexed_rule->redirect_url = std::move(*redirect.url);
    return ParseResult::SUCCESS;
  }

  if (redirect.extension_path) {
    if (!IsRedirectUrlRelative(*redirect.extension_path))
      return ParseResult::ERROR_INVALID_EXTENSION_PATH;

    GURL redirect_url = base_url.Resolve(*redirect.extension_path);

    // Sanity check that Resolve works as expected.
    DCHECK_EQ(base_url.GetOrigin(), redirect_url.GetOrigin());

    if (!redirect_url.is_valid())
      return ParseResult::ERROR_INVALID_EXTENSION_PATH;

    indexed_rule->redirect_url = redirect_url.spec();
    return ParseResult::SUCCESS;
  }

  if (redirect.transform) {
    indexed_rule->url_transform = std::move(redirect.transform);
    return ValidateTransform(*indexed_rule->url_transform);
  }

  if (redirect.regex_substitution) {
    if (redirect.regex_substitution->empty())
      return ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION;

    indexed_rule->regex_substitution = std::move(*redirect.regex_substitution);
    return ParseResult::SUCCESS;
  }

  return ParseResult::ERROR_INVALID_REDIRECT;
}

bool DoesActionSupportPriority(dnr_api::RuleActionType type) {
  switch (type) {
    case dnr_api::RULE_ACTION_TYPE_BLOCK:
    case dnr_api::RULE_ACTION_TYPE_REDIRECT:
    case dnr_api::RULE_ACTION_TYPE_ALLOW:
    case dnr_api::RULE_ACTION_TYPE_UPGRADESCHEME:
    case dnr_api::RULE_ACTION_TYPE_ALLOWALLREQUESTS:
    case dnr_api::RULE_ACTION_TYPE_MODIFYHEADERS:
      return true;
    case dnr_api::RULE_ACTION_TYPE_NONE:
      break;
  }
  NOTREACHED();
  return false;
}

uint8_t GetActionTypePriority(dnr_api::RuleActionType action_type) {
  switch (action_type) {
    case dnr_api::RULE_ACTION_TYPE_ALLOW:
      return 5;
    case dnr_api::RULE_ACTION_TYPE_ALLOWALLREQUESTS:
      return 4;
    case dnr_api::RULE_ACTION_TYPE_BLOCK:
      return 3;
    case dnr_api::RULE_ACTION_TYPE_UPGRADESCHEME:
      return 2;
    case dnr_api::RULE_ACTION_TYPE_REDIRECT:
      return 1;
    case dnr_api::RULE_ACTION_TYPE_MODIFYHEADERS:
      return 0;
    case dnr_api::RULE_ACTION_TYPE_NONE:
      break;
  }
  NOTREACHED();
  return 0;
}

void RecordLargeRegexUMA(bool is_large_regex) {
  UMA_HISTOGRAM_BOOLEAN(kIsLargeRegexHistogram, is_large_regex);
}

ParseResult ValidateHeaders(
    const std::vector<dnr_api::ModifyHeaderInfo>& headers,
    bool are_request_headers) {
  if (headers.empty()) {
    return are_request_headers ? ParseResult::ERROR_EMPTY_REQUEST_HEADERS_LIST
                               : ParseResult::ERROR_EMPTY_RESPONSE_HEADERS_LIST;
  }

  for (const auto& header_info : headers) {
    if (!net::HttpUtil::IsValidHeaderName(header_info.header))
      return ParseResult::ERROR_INVALID_HEADER_NAME;
  }

  return ParseResult::SUCCESS;
}

}  // namespace

IndexedRule::IndexedRule() = default;
IndexedRule::~IndexedRule() = default;
IndexedRule::IndexedRule(IndexedRule&& other) = default;
IndexedRule& IndexedRule::operator=(IndexedRule&& other) = default;

// static
ParseResult IndexedRule::CreateIndexedRule(dnr_api::Rule parsed_rule,
                                           const GURL& base_url,
                                           IndexedRule* indexed_rule) {
  DCHECK(indexed_rule);

  if (parsed_rule.id < kMinValidID)
    return ParseResult::ERROR_INVALID_RULE_ID;

  const bool is_priority_supported =
      DoesActionSupportPriority(parsed_rule.action.type);
  if (is_priority_supported) {
    if (!parsed_rule.priority)
      return ParseResult::ERROR_EMPTY_RULE_PRIORITY;
    if (*parsed_rule.priority < kMinValidPriority)
      return ParseResult::ERROR_INVALID_RULE_PRIORITY;
  }

  const bool is_redirect_rule =
      parsed_rule.action.type == dnr_api::RULE_ACTION_TYPE_REDIRECT;

  if (is_redirect_rule) {
    if (!parsed_rule.action.redirect)
      return ParseResult::ERROR_INVALID_REDIRECT;

    ParseResult result = ParseRedirect(std::move(*parsed_rule.action.redirect),
                                       base_url, indexed_rule);
    if (result != ParseResult::SUCCESS)
      return result;
  }

  if (parsed_rule.condition.domains && parsed_rule.condition.domains->empty())
    return ParseResult::ERROR_EMPTY_DOMAINS_LIST;

  if (parsed_rule.condition.resource_types &&
      parsed_rule.condition.resource_types->empty()) {
    return ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST;
  }

  if (parsed_rule.condition.url_filter && parsed_rule.condition.regex_filter)
    return ParseResult::ERROR_MULTIPLE_FILTERS_SPECIFIED;

  const bool is_regex_rule = !!parsed_rule.condition.regex_filter;

  if (!is_regex_rule && indexed_rule->regex_substitution)
    return ParseResult::ERROR_REGEX_SUBSTITUTION_WITHOUT_FILTER;

  if (is_regex_rule) {
    if (parsed_rule.condition.regex_filter->empty())
      return ParseResult::ERROR_EMPTY_REGEX_FILTER;

    if (!base::IsStringASCII(*parsed_rule.condition.regex_filter))
      return ParseResult::ERROR_NON_ASCII_REGEX_FILTER;

    bool require_capturing = indexed_rule->regex_substitution.has_value();

    // TODO(karandeepb): Regex compilation can be expensive. Also, these need to
    // be compiled again once the ruleset is loaded, which means duplicate work.
    // We should maintain a global cache of compiled regexes.
    re2::RE2 regex(
        *parsed_rule.condition.regex_filter,
        CreateRE2Options(IsCaseSensitive(parsed_rule), require_capturing));

    if (regex.error_code() == re2::RE2::ErrorPatternTooLarge) {
      RecordLargeRegexUMA(true);
      return ParseResult::ERROR_REGEX_TOO_LARGE;
    }

    if (!regex.ok())
      return ParseResult::ERROR_INVALID_REGEX_FILTER;

    std::string error;
    if (indexed_rule->regex_substitution &&
        !regex.CheckRewriteString(*indexed_rule->regex_substitution, &error)) {
      return ParseResult::ERROR_INVALID_REGEX_SUBSTITUTION;
    }

    RecordLargeRegexUMA(false);
  }

  if (parsed_rule.condition.url_filter) {
    if (parsed_rule.condition.url_filter->empty())
      return ParseResult::ERROR_EMPTY_URL_FILTER;

    if (!base::IsStringASCII(*parsed_rule.condition.url_filter))
      return ParseResult::ERROR_NON_ASCII_URL_FILTER;
  }

  indexed_rule->action_type = parsed_rule.action.type;
  indexed_rule->id = base::checked_cast<uint32_t>(parsed_rule.id);
  indexed_rule->priority = parsed_rule.priority ? ComputeIndexedRulePriority(
                                                      *parsed_rule.priority,
                                                      indexed_rule->action_type)
                                                : kDefaultPriority;
  indexed_rule->options = GetOptionsMask(parsed_rule);
  indexed_rule->activation_types = GetActivationTypes(parsed_rule);

  {
    ParseResult result =
        ComputeElementTypes(parsed_rule, &indexed_rule->element_types);
    if (result != ParseResult::SUCCESS)
      return result;
  }

  if (!CanonicalizeDomains(std::move(parsed_rule.condition.domains),
                           &indexed_rule->domains)) {
    return ParseResult::ERROR_NON_ASCII_DOMAIN;
  }

  if (!CanonicalizeDomains(std::move(parsed_rule.condition.excluded_domains),
                           &indexed_rule->excluded_domains)) {
    return ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN;
  }

  if (is_regex_rule) {
    indexed_rule->url_pattern_type =
        url_pattern_index::flat::UrlPatternType_REGEXP;
    indexed_rule->url_pattern = std::move(*parsed_rule.condition.regex_filter);
  } else {
    // Parse the |anchor_left|, |anchor_right|, |url_pattern_type| and
    // |url_pattern| fields.
    UrlFilterParser::Parse(std::move(parsed_rule.condition.url_filter),
                           indexed_rule);
  }

  // url_pattern_index doesn't support patterns starting with a domain anchor
  // followed by a wildcard, e.g. ||*xyz.
  if (indexed_rule->anchor_left == flat_rule::AnchorType_SUBDOMAIN &&
      !indexed_rule->url_pattern.empty() &&
      indexed_rule->url_pattern.front() == kWildcardCharacter) {
    return ParseResult::ERROR_INVALID_URL_FILTER;
  }

  // Lower-case case-insensitive patterns as required by url pattern index.
  if (indexed_rule->options & flat_rule::OptionFlag_IS_CASE_INSENSITIVE)
    indexed_rule->url_pattern = base::ToLowerASCII(indexed_rule->url_pattern);

  if (parsed_rule.action.type == dnr_api::RULE_ACTION_TYPE_MODIFYHEADERS) {
    if (!parsed_rule.action.request_headers &&
        !parsed_rule.action.response_headers)
      return ParseResult::ERROR_NO_HEADERS_SPECIFIED;

    if (parsed_rule.action.request_headers) {
      indexed_rule->request_headers =
          std::move(*parsed_rule.action.request_headers);

      ParseResult result = ValidateHeaders(indexed_rule->request_headers,
                                           true /* are_request_headers */);
      if (result != ParseResult::SUCCESS)
        return result;
    }

    if (parsed_rule.action.response_headers) {
      indexed_rule->response_headers =
          std::move(*parsed_rule.action.response_headers);

      ParseResult result = ValidateHeaders(indexed_rule->response_headers,
                                           false /* are_request_headers */);
      if (result != ParseResult::SUCCESS)
        return result;
    }
  }

  // Some sanity checks to ensure we return a valid IndexedRule.
  DCHECK_GE(indexed_rule->id, static_cast<uint32_t>(kMinValidID));
  DCHECK_GE(indexed_rule->priority, static_cast<uint32_t>(kMinValidPriority));
  DCHECK(IsSubset(indexed_rule->options, flat_rule::OptionFlag_ANY));
  DCHECK(IsSubset(indexed_rule->element_types, flat_rule::ElementType_ANY));
  DCHECK_EQ(flat_rule::ActivationType_NONE, indexed_rule->activation_types);
  DCHECK_NE(flat_rule::AnchorType_SUBDOMAIN, indexed_rule->anchor_right);

  return ParseResult::SUCCESS;
}

uint64_t ComputeIndexedRulePriority(int parsed_rule_priority,
                                    dnr_api::RuleActionType action_type) {
  if (!DoesActionSupportPriority(action_type))
    return kDefaultPriority;
  // Incorporate the action's priority into the rule priority, so e.g. allow
  // rules will be given a higher priority than block rules with the same
  // priority specified in the rule JSON.
  return (base::checked_cast<uint32_t>(parsed_rule_priority) << 8) |
         GetActionTypePriority(action_type);
}

}  // namespace declarative_net_request
}  // namespace extensions
