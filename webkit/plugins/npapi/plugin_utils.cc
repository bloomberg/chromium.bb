// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_utils.h"

#include <algorithm>

#include "base/string_split.h"
#include "base/string_util.h"
#include "base/version.h"

namespace webkit {
namespace npapi {

void CreateVersionFromString(const string16& version_string,
                             Version* parsed_version) {
  // Remove spaces and ')' from the version string,
  // Replace any instances of 'r', ',' or '(' with a dot.
  std::string version = UTF16ToASCII(version_string);
  RemoveChars(version, ") ", &version);
  std::replace(version.begin(), version.end(), 'd', '.');
  std::replace(version.begin(), version.end(), 'r', '.');
  std::replace(version.begin(), version.end(), ',', '.');
  std::replace(version.begin(), version.end(), '(', '.');
  std::replace(version.begin(), version.end(), '_', '.');

  // Remove leading zeros from each of the version components.
  std::string no_leading_zeros_version;
  std::vector<std::string> numbers;
  base::SplitString(version, '.', &numbers);
  for (size_t i = 0; i < numbers.size(); ++i) {
    size_t n = numbers[i].size();
    size_t j = 0;
    while (j < n && numbers[i][j] == '0') {
      ++j;
    }
    no_leading_zeros_version += (j < n) ? numbers[i].substr(j) : "0";
    if (i != numbers.size() - 1) {
      no_leading_zeros_version += ".";
    }
  }

  *parsed_version = Version(no_leading_zeros_version);
}

}  // namespace npapi
}  // namespace webkit
