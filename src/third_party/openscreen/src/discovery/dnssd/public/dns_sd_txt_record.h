// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_DNS_SD_TXT_RECORD_H_
#define DISCOVERY_DNSSD_PUBLIC_DNS_SD_TXT_RECORD_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

class DnsSdTxtRecord {
 public:
  // Sets the value currently stored in this DNS-SD TXT record. Returns error
  // if the provided key is already set or if either the key or value is
  // invalid, and Error::None() otherwise. Keys are case-insensitive. Setting a
  // value or flag which was already set will overwrite the previous one, and
  // setting a value with a key which was previously associated with a flag
  // erases the flag's value and vice versa.
  Error SetValue(const std::string& key,
                 const absl::Span<const uint8_t>& value);
  Error SetFlag(const std::string& key, bool value);

  // Reads the value associated with the provided key, or an error if the key
  // is mapped to the opposite type or the query is otherwise invalid. Keys are
  // case-insensitive.
  // NOTE: If GetValue is called on a key assigned to a flag, an ItemNotFound
  // error will be returned. If GetFlag is called on a key assigned to a value,
  // 'false' will be returned.
  ErrorOr<absl::Span<const uint8_t>> GetValue(const std::string& key) const;
  ErrorOr<bool> GetFlag(const std::string& key) const;

  // Clears an existing TxtRecord value associated with the given key. If the
  // key is not found, these methods return successfully without removing any
  // keys.
  // NOTE: If ClearValue is called on a key assigned to a flag (or ClearFlag for
  // a value), no error will be returned but no value will be cleared.
  Error ClearValue(const std::string& key);
  Error ClearFlag(const std::string& key);

  inline bool IsEmpty() const {
    return key_value_txt_.empty() && boolean_txt_.empty();
  }

  // Returns the data for the TXT record represented by this object.
  // Specifically, it returns a vector containing an entry for each string s in
  // boolean_txt_ of the form 's' (without quotes) and an entry for each key-
  // value pair (key, value) in key_value_txt_ of the form 'key=value' (without
  // quotes).
  std::vector<std::vector<uint8_t>> GetData() const;

  inline bool operator==(const DnsSdTxtRecord& other) const {
    return key_value_txt_ == other.key_value_txt_ &&
           boolean_txt_ == other.boolean_txt_;
  }

  inline bool operator!=(const DnsSdTxtRecord& other) const {
    return !(*this == other);
  }

 private:
  struct CaseInsensitiveComparison {
    bool operator()(const std::string& lhs, const std::string& rhs) const;
  };

  // Validations for keys and (key, value) pairs.
  bool IsKeyValid(const std::string& key) const;
  bool IsKeyValuePairValid(const std::string& key,
                           const absl::Span<const uint8_t>& value) const;

  // Set of (key, value) pairs associated with this TXT record.
  // NOTE: The same string name can only occur in one of key_value_txt_,
  // boolean_txt_.
  std::map<std::string, std::vector<uint8_t>, CaseInsensitiveComparison>
      key_value_txt_;

  // Set of named booleans associated with this TXT record. All stored boolean
  // names are 'true', as 'false' values are not stored.
  // NOTE: The same string name can only occur in one of key_value_txt_,
  // boolean_txt_.
  std::set<std::string, CaseInsensitiveComparison> boolean_txt_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_DNS_SD_TXT_RECORD_H_
