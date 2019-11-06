// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/embedder/shared_file_util.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

namespace service_manager {

void SharedFileSwitchValueBuilder::AddEntry(const std::string& key_str,
                                            int key_id) {
  if (!switch_value_.empty()) {
    switch_value_ += ",";
  }
  switch_value_ += key_str, switch_value_ += ":";
  switch_value_ += base::NumberToString(key_id);
}

base::Optional<std::map<int, std::string>> ParseSharedFileSwitchValue(
    const std::string& value) {
  std::map<int, std::string> values;
  std::vector<std::string> string_pairs = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& pair : string_pairs) {
    size_t colon_position = pair.find(":");
    if (colon_position == std::string::npos || colon_position == 0 ||
        colon_position == pair.size() - 1) {
      DLOG(ERROR) << "Found invalid entry parsing shared file string value:"
                  << pair;
      return base::nullopt;
    }
    std::string key = pair.substr(0, colon_position);
    std::string number_string =
        pair.substr(colon_position + 1, std::string::npos);
    int key_int;
    if (!base::StringToInt(number_string, &key_int)) {
      DLOG(ERROR) << "Found invalid entry parsing shared file string value:"
                  << number_string << " (not an int).";
      return base::nullopt;
    }

    values[key_int] = key;
  }
  return base::make_optional(std::move(values));
}

}  // namespace service_manager
