// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "third_party/re2/src/re2/re2.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
struct WebRequestInfo;

namespace declarative_net_request {
class CompositeMatcher;

// Returns the version header used for indexed ruleset files. Only exposed for
// testing.
std::string GetVersionHeaderForTesting();

// Gets the ruleset format version for testing.
int GetIndexedRulesetFormatVersionForTesting();

// Test helper to increment the indexed ruleset format version while the
// returned value is in scope. Resets it to the original value when it goes out
// of scope.
using ScopedIncrementRulesetVersion = base::AutoReset<int>;
ScopedIncrementRulesetVersion CreateScopedIncrementRulesetVersionForTesting();

// Strips the version header from |ruleset_data|. Returns false on version
// mismatch.
bool StripVersionHeaderAndParseVersion(std::string* ruleset_data);

// Returns the checksum of the given serialized |data|. |data| must not include
// the version header.
int GetChecksum(base::span<const uint8_t> data);

// Override the result of any calls to GetChecksum() above, so that it returns
// |checksum|. Note: If |checksum| is -1, no such override is performed.
void OverrideGetChecksumForTest(int checksum);

// Helper function to persist the indexed ruleset |data| at the given |path|.
// The ruleset is composed of a version header corresponding to the current
// ruleset format version, followed by the actual ruleset data.
bool PersistIndexedRuleset(const base::FilePath& path,
                           base::span<const uint8_t> data);

// Helper to clear each renderer's in-memory cache the next time it navigates.
void ClearRendererCacheOnNavigation();

// Helper to log the |kReadDynamicRulesJSONStatusHistogram| histogram.
void LogReadDynamicRulesStatus(ReadJSONRulesResult::Status status);

// Constructs an api::declarative_net_request::RequestDetails from a
// WebRequestInfo.
api::declarative_net_request::RequestDetails CreateRequestDetails(
    const WebRequestInfo& request);

// Creates default RE2::Options.
re2::RE2::Options CreateRE2Options(bool is_case_sensitive,
                                   bool require_capturing);

// Convert dnr_api::RuleActionType into flat::ActionType.
flat::ActionType ConvertToFlatActionType(
    api::declarative_net_request::RuleActionType action_type);

// Returns the extension-specified ID for the given |ruleset_id| if it
// corresponds to a static ruleset ID. For the dynamic or session-scoped ruleset
// ID, it returns the |DYNAMIC_RULESET_ID| and |SESSION_RULESET_ID| API
// constants respectively.
std::string GetPublicRulesetID(const Extension& extension,
                               RulesetID ruleset_id);

// Returns the public ruleset IDs corresponding to the given |extension| and
// |matcher|.
std::vector<std::string> GetPublicRulesetIDs(const Extension& extension,
                                             const CompositeMatcher& matcher);

// Returns the number of rules that an extension can specify across its enabled
// static rulesets that will not count towards the global total.
int GetStaticGuaranteedMinimumRuleCount();

// Returns the maximum amount of static rules in the global rule pool for a
// single profile.
int GetGlobalStaticRuleLimit();

// Returns the maximum number of rules a valid static ruleset can have. This is
// also the maximum number of static rules an extension can enable at any point.
int GetMaximumRulesPerRuleset();

// Returns the shared rule limit for dynamic and session-scoped rules.
int GetDynamicAndSessionRuleLimit();

// Returns the per-extension regex rules limit. This is enforced separately for
// static and dynamic rulesets.
int GetRegexRuleLimit();

// Test helpers to override the various rule limits until the returned value is
// in scope.
using ScopedRuleLimitOverride = base::AutoReset<int>;
ScopedRuleLimitOverride CreateScopedStaticGuaranteedMinimumOverrideForTesting(
    int minimum);
ScopedRuleLimitOverride CreateScopedGlobalStaticRuleLimitOverrideForTesting(
    int limit);
ScopedRuleLimitOverride CreateScopedRegexRuleLimitOverrideForTesting(int limit);
ScopedRuleLimitOverride
CreateScopedDynamicAndSessionRuleLimitOverrideForTesting(int limit);

// Helper to convert a flatbufffers::String to a string-like object with type T.
template <typename T>
T CreateString(const flatbuffers::String& str) {
  return T(str.c_str(), str.size());
}

// Returns the number of static rules enabled for the specified
// |composite_matcher|.
size_t GetEnabledStaticRuleCount(const CompositeMatcher* composite_matcher);

// Returns true if |extension| has the declarativeNetRequestFeedback permission
// for the specified |tab_id|. If |tab_is| is omitted, then non-tab specific
// permissions are checked.
bool HasDNRFeedbackPermission(const Extension* extension,
                              const base::Optional<int>& tab_id);

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_
