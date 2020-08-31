// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/optional.h"
#include "extensions/browser/api/declarative_net_request/constants.h"

namespace extensions {
namespace declarative_net_request {

// Holds the result of indexing a JSON ruleset.
class ParseInfo {
 public:
  // Constructor to be used on success.
  ParseInfo(size_t rules_count,
            size_t regex_rules_count,
            int ruleset_checksum,
            std::vector<int> regex_limit_exceeded_rules);

  // Constructor to be used on error.
  ParseInfo(ParseResult error_reason, const int* rule_id);

  ParseInfo(ParseInfo&&);
  ParseInfo& operator=(ParseInfo&&);
  ~ParseInfo();


  bool has_error() const { return has_error_; }
  ParseResult error_reason() const {
    DCHECK(has_error_);
    return error_reason_;
  }
  const std::string& error() const {
    DCHECK(has_error_);
    return error_;
  }

  // Rules which exceed the per-rule regex memory limit. These are ignored
  // during indexing.
  const std::vector<int>& regex_limit_exceeded_rules() const {
    DCHECK(!has_error_);
    return regex_limit_exceeded_rules_;
  }

  size_t rules_count() const {
    DCHECK(!has_error_);
    return rules_count_;
  }

  size_t regex_rules_count() const {
    DCHECK(!has_error_);
    return regex_rules_count_;
  }

  int ruleset_checksum() const {
    DCHECK(!has_error_);
    return ruleset_checksum_;
  }

 private:
  bool has_error_ = false;

  // Only valid iff |has_error_| is true.
  std::string error_;
  ParseResult error_reason_ = ParseResult::NONE;

  // Only valid iff |has_error_| is false.
  size_t rules_count_ = 0;
  size_t regex_rules_count_ = 0;
  int ruleset_checksum_ = -1;
  std::vector<int> regex_limit_exceeded_rules_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_PARSE_INFO_H_
