// Copyright (C) 2014 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "string_util.h"

#include <libaddressinput/util/scoped_ptr.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "canonicalize_string.h"

#ifdef _MSC_VER
// http://msdn.microsoft.com/en-us/library/2ts7cx93%28v=vs.110%29.aspx
#define snprintf _snprintf
#endif  // _MSC_VER

namespace i18n {
namespace addressinput {

std::string NormalizeLanguageCode(const std::string& language_code) {
  std::string lowercase = language_code;
  std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
                 tolower);
  std::string::size_type pos = lowercase.find('-');
  if (pos == std::string::npos) {
    return language_code;
  }
  static const char kLatinSuffix[] = "-latn";
  static const size_t kLatinSuffixSize = sizeof kLatinSuffix - 1;
  if (lowercase.substr(pos, kLatinSuffixSize) == kLatinSuffix) {
    return language_code;
  }
  return lowercase.substr(0, pos);
}

std::string TimeToString(time_t time) {
  char time_string[2 + 3 * sizeof time];
  snprintf(time_string, sizeof time_string, "%ld", time);
  return time_string;
}

bool LooseStringCompare(const std::string& a, const std::string& b) {
  scoped_ptr<StringCanonicalizer> canonicalizer(StringCanonicalizer::Build());
  return canonicalizer->CanonicalizeString(a) ==
         canonicalizer->CanonicalizeString(b);
}

// The original source code is from:
// http://src.chromium.org/viewvc/chrome/trunk/src/base/strings/string_split.cc?revision=216633
void SplitString(const std::string& str, char s, std::vector<std::string>* r) {
  assert(r != NULL);
  r->clear();
  size_t last = 0;
  size_t c = str.size();
  for (size_t i = 0; i <= c; ++i) {
    if (i == c || str[i] == s) {
      std::string tmp(str, last, i - last);
      // Avoid converting an empty or all-whitespace source string into a vector
      // of one empty string.
      if (i != c || !r->empty() || !tmp.empty()) {
        r->push_back(tmp);
      }
      last = i + 1;
    }
  }
}

}  // namespace addressinput
}  // namespace i18n
