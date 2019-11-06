// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/test_ruleset_utils.h"

#include <utility>

namespace subresource_filter {
namespace testing {

namespace proto = url_pattern_index::proto;

proto::UrlRule CreateSuffixRule(base::StringPiece suffix) {
  proto::UrlRule rule;
  rule.set_semantics(proto::RULE_SEMANTICS_BLACKLIST);
  rule.set_source_type(proto::SOURCE_TYPE_ANY);
  rule.set_element_types(proto::ELEMENT_TYPE_ALL);
  rule.set_url_pattern_type(proto::URL_PATTERN_TYPE_SUBSTRING);
  rule.set_anchor_left(proto::ANCHOR_TYPE_NONE);
  rule.set_anchor_right(proto::ANCHOR_TYPE_BOUNDARY);
  rule.set_url_pattern(suffix.as_string());
  return rule;
}

proto::UrlRule CreateWhitelistSuffixRule(base::StringPiece suffix) {
  proto::UrlRule rule;
  rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
  rule.set_source_type(proto::SOURCE_TYPE_ANY);
  rule.set_element_types(proto::ELEMENT_TYPE_ALL);
  rule.set_url_pattern_type(proto::URL_PATTERN_TYPE_SUBSTRING);
  rule.set_anchor_left(proto::ANCHOR_TYPE_NONE);
  rule.set_anchor_right(proto::ANCHOR_TYPE_BOUNDARY);
  rule.set_url_pattern(suffix.as_string());
  return rule;
}

proto::UrlRule CreateWhitelistRuleForDocument(
    base::StringPiece pattern,
    int32_t activation_types,
    std::vector<std::string> domains) {
  proto::UrlRule rule;
  rule.set_semantics(proto::RULE_SEMANTICS_WHITELIST);
  rule.set_source_type(proto::SOURCE_TYPE_ANY);
  rule.set_activation_types(activation_types);

  for (std::string& domain : domains) {
    rule.add_domains()->set_domain(std::move(domain));
  }

  rule.set_url_pattern_type(proto::URL_PATTERN_TYPE_SUBSTRING);
  rule.set_anchor_left(proto::ANCHOR_TYPE_NONE);
  rule.set_anchor_right(proto::ANCHOR_TYPE_NONE);
  rule.set_url_pattern(pattern.as_string());
  return rule;
}

}  // namespace testing
}  // namespace subresource_filter
