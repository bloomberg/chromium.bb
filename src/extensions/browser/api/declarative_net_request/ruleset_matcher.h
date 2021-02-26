// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "extensions/browser/api/declarative_net_request/extension_url_pattern_index_matcher.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/regex_rules_matcher.h"
#include "extensions/common/api/declarative_net_request/constants.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions {

namespace declarative_net_request {
class RulesetSource;
enum class LoadRulesetResult;

namespace flat {
struct ExtensionIndexedRuleset;
struct UrlRuleMetadata;
}  // namespace flat

// RulesetMatcher encapsulates the Declarative Net Request API ruleset
// corresponding to a single RulesetSource. Since this class is immutable, it is
// thread-safe.
// TODO(karandeepb): Rename to RulesetSourceMatcher since this no longer
// inherits from RulesetMatcherBase.
class RulesetMatcher {
 public:
  // Factory function to create a verified RulesetMatcher for |source|. Must be
  // called on a sequence where file IO is allowed. Returns kSuccess on
  // success along with the ruleset |matcher|.
  static LoadRulesetResult CreateVerifiedMatcher(
      const RulesetSource& source,
      int expected_ruleset_checksum,
      std::unique_ptr<RulesetMatcher>* matcher);

  ~RulesetMatcher();

  base::Optional<RequestAction> GetBeforeRequestAction(
      const RequestParams& params) const;

  // Returns a list of actions corresponding to all matched
  // modifyHeaders rules with priority greater than |min_priority| if specified.
  std::vector<RequestAction> GetModifyHeadersActions(
      const RequestParams& params,
      base::Optional<uint64_t> min_priority) const;

  bool IsExtraHeadersMatcher() const;
  size_t GetRulesCount() const;
  size_t GetRegexRulesCount() const;

  void OnRenderFrameCreated(content::RenderFrameHost* host);
  void OnRenderFrameDeleted(content::RenderFrameHost* host);
  void OnDidFinishNavigation(content::RenderFrameHost* host);

  // ID of the ruleset. Each extension can have multiple rulesets with
  // their own unique ids.
  RulesetID id() const { return id_; }

  // Returns the tracked highest priority matching allowsAllRequests action, if
  // any, for |host|.
  base::Optional<RequestAction> GetAllowlistedFrameActionForTesting(
      content::RenderFrameHost* host) const;

 private:
  explicit RulesetMatcher(std::string ruleset_data,
                          RulesetID id,
                          const ExtensionId& extension_id);

  const std::string ruleset_data_;

  const flat::ExtensionIndexedRuleset* const root_;

  const RulesetID id_;

  // Underlying matcher for filter-list style rules supported using the
  // |url_pattern_index| component.
  ExtensionUrlPatternIndexMatcher url_pattern_index_matcher_;

  // Underlying matcher for regex rules.
  RegexRulesMatcher regex_matcher_;

  DISALLOW_COPY_AND_ASSIGN(RulesetMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_H_
