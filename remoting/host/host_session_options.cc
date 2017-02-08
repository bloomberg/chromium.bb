// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_session_options.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace remoting {

namespace {

static constexpr char kSeparator = ',';
static constexpr char kKeyValueSeparator = ':';

// Whether |value| is good to be added to HostSessionOptions as a value.
bool ValueIsValid(const std::string& value) {
  return value.find(kSeparator) == std::string::npos &&
         value.find(kKeyValueSeparator) == std::string::npos &&
         base::IsStringASCII(value);
}

// Whether |key| is good to be added to HostSessionOptions as a key.
bool KeyIsValid(const std::string& key) {
  return !key.empty() && ValueIsValid(key);
}

}  // namespace

HostSessionOptions::HostSessionOptions() = default;
HostSessionOptions::~HostSessionOptions() = default;

HostSessionOptions::HostSessionOptions(const std::string& parameter) {
  Import(parameter);
}

void HostSessionOptions::Append(const std::string& key,
                                const std::string& value) {
  DCHECK(KeyIsValid(key));
  DCHECK(ValueIsValid(value));
  options_[key] = value;
}

base::Optional<std::string> HostSessionOptions::Get(
    const std::string& key) const {
  auto it = options_.find(key);
  if (it == options_.end()) {
    return base::nullopt;
  }
  return it->second;
}

std::string HostSessionOptions::Export() const {
  std::string result;
  for (const auto& pair : options_) {
    if (!result.empty()) {
      result += kSeparator;
    }
    if (!pair.first.empty()) {
      result += pair.first;
      result.push_back(kKeyValueSeparator);
      result += pair.second;
    }
  }
  return result;
}

void HostSessionOptions::Import(const std::string& parameter) {
  options_.clear();
  std::vector<std::pair<std::string, std::string>> result;
  base::SplitStringIntoKeyValuePairs(parameter,
                                     kKeyValueSeparator,
                                     kSeparator,
                                     &result);
  for (const auto& pair : result) {
    if (KeyIsValid(pair.first) && ValueIsValid(pair.second)) {
      Append(pair.first, pair.second);
    }
  }
}

}  // namespace remoting
