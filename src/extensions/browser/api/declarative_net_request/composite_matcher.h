// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/common/permissions/permissions_data.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions {
namespace declarative_net_request {

struct RequestAction;

// Per extension instance which manages the different rulesets for an extension
// while respecting their priorities.
class CompositeMatcher {
 public:
  struct ActionInfo {
    ActionInfo(base::Optional<RequestAction> action, bool notify);
    ~ActionInfo();
    ActionInfo(ActionInfo&& other);
    ActionInfo& operator=(ActionInfo&& other);

    // The action to be taken for this request.
    base::Optional<RequestAction> action;

    // Whether the extension should be notified that the request was unable to
    // be redirected as the extension lacks the appropriate host permission for
    // the request. Can only be true for redirect actions.
    bool notify_request_withheld = false;

    DISALLOW_COPY_AND_ASSIGN(ActionInfo);
  };

  using MatcherList = std::vector<std::unique_ptr<RulesetMatcher>>;

  // Each RulesetMatcher should have a distinct ID and priority.
  explicit CompositeMatcher(MatcherList matchers);
  ~CompositeMatcher();

  const MatcherList& matchers() const { return matchers_; }

  // Returns the set of matchers and resets |matchers_| to an empty vector.
  MatcherList GetAndResetMatchers();

  // Updates the set of matchers. IDs for all the |matchers| must be unique.
  void SetMatchers(MatcherList matchers);

  // Adds the |new_matcher| to the list of matchers. If a matcher with the
  // corresponding ID is already present, updates the matcher.
  void AddOrUpdateRuleset(std::unique_ptr<RulesetMatcher> new_matcher);

  // Computes and returns the set of static RulesetIDs corresponding to
  // |matchers_|.
  std::set<RulesetID> ComputeStaticRulesetIDs() const;

  // Returns a RequestAction for the network request specified by |params|, or
  // base::nullopt if there is no matching rule.
  ActionInfo GetBeforeRequestAction(
      const RequestParams& params,
      PermissionsData::PageAccess page_access) const;

  // Returns all matching RequestActions for the request corresponding to
  // modifyHeaders rules matched from this extension, sorted in descending order
  // by rule priority.
  std::vector<RequestAction> GetModifyHeadersActions(
      const RequestParams& params) const;

  // Returns whether this modifies "extraHeaders".
  bool HasAnyExtraHeadersMatcher() const;

  void OnRenderFrameCreated(content::RenderFrameHost* host);
  void OnRenderFrameDeleted(content::RenderFrameHost* host);
  void OnDidFinishNavigation(content::RenderFrameHost* host);

 private:
  // This must be called whenever |matchers_| are modified.
  void OnMatchersModified();

  bool ComputeHasAnyExtraHeadersMatcher() const;

  // The RulesetMatchers, in an arbitrary order.
  MatcherList matchers_;

  // Denotes the cached return value for |HasAnyExtraHeadersMatcher|. Care must
  // be taken to reset this as this object is modified.
  mutable base::Optional<bool> has_any_extra_headers_matcher_;

  DISALLOW_COPY_AND_ASSIGN(CompositeMatcher);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_COMPOSITE_MATCHER_H_
