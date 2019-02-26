// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"

namespace base {
class ListValue;
class Token;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace extensions {
class Extension;
struct InstallWarning;

namespace declarative_net_request {

struct IndexAndPersistRulesResult {
 public:
  static IndexAndPersistRulesResult CreateSuccessResult(
      int ruleset_checksum,
      std::vector<InstallWarning> warnings);
  static IndexAndPersistRulesResult CreateErrorResult(std::string error);

  ~IndexAndPersistRulesResult();
  IndexAndPersistRulesResult(IndexAndPersistRulesResult&&);
  IndexAndPersistRulesResult& operator=(IndexAndPersistRulesResult&&);

  // Whether IndexAndPersistRules succeeded.
  bool success;

  // Checksum of the persisted indexed ruleset file. Valid if |success| if true.
  int ruleset_checksum;

  // Valid if |success| is true.
  std::vector<InstallWarning> warnings;

  // Valid if |success| is false.
  std::string error;

 private:
  IndexAndPersistRulesResult();
  DISALLOW_COPY_AND_ASSIGN(IndexAndPersistRulesResult);
};

// Indexes and persists the JSON ruleset for for |extension|. This is
// potentially unsafe since the JSON rules file is parsed in-process. Should
// only be called for an extension which provided a JSON ruleset.
// Note: This must be called on a sequence where file IO is allowed.
IndexAndPersistRulesResult IndexAndPersistRulesUnsafe(
    const Extension& extension);

using IndexAndPersistRulesCallback =
    base::OnceCallback<void(IndexAndPersistRulesResult)>;
// Same as IndexAndPersistRulesUnsafe but parses the JSON rules file out-of-
// process. |connector| should be a connector to the ServiceManager usable on
// the current sequence. Optionally clients can pass a valid |decoder_batch_id|
// to be used when accessing the data decoder service, which is used internally
// to parse JSON.
//
// NOTE: This must be called on a sequence where file IO is allowed.
void IndexAndPersistRules(service_manager::Connector* connector,
                          const base::Optional<base::Token>& decoder_batch_id,
                          const Extension& extension,
                          IndexAndPersistRulesCallback callback);

// Returns true if |data| represents a valid data buffer containing indexed
// ruleset data with |expected_checksum|.
bool IsValidRulesetData(base::span<const uint8_t> data, int expected_checksum);

// Returns the version header used for indexed ruleset files. Only exposed for
// testing.
std::string GetVersionHeaderForTesting();

// Returns the indexed ruleset format version.
int GetIndexedRulesetFormatVersionForTesting();

// Override the ruleset format version for testing.
void SetIndexedRulesetFormatVersionForTesting(int version);

// Strips the version header from |ruleset_data|. Returns false on version
// mismatch.
bool StripVersionHeaderAndParseVersion(std::string* ruleset_data);

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_
