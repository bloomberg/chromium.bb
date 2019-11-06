// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class Extension;

namespace declarative_net_request {

class RulesetSource;
class RulesetMatcher;
struct TestRule;

// Enum specifying the extension load type. Used for parameterized tests.
enum class ExtensionLoadType {
  PACKED,
  UNPACKED,
};

// Returns true if the given extension has a valid indexed ruleset. Should be
// called on a sequence where file IO is allowed.
bool HasValidIndexedRuleset(const Extension& extension,
                            content::BrowserContext* browser_context);

// Helper to create a verified ruleset matcher. Populates |matcher| and
// |expected_checksum|. Returns true on success.
bool CreateVerifiedMatcher(const std::vector<TestRule>& rules,
                           const RulesetSource& source,
                           std::unique_ptr<RulesetMatcher>* matcher,
                           int* expected_checksum = nullptr);

// Helper to return a RulesetSource bound to temporary files.
RulesetSource CreateTemporarySource(size_t id = 1,
                                    size_t priority = 1,
                                    size_t rule_count_limit = 100,
                                    ExtensionId extension_id = "extensionid");

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
