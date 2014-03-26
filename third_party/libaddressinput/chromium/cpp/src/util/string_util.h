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

#ifndef I18N_ADDRESSINPUT_UTIL_STRING_UTIL_H_
#define I18N_ADDRESSINPUT_UTIL_STRING_UTIL_H_

#include <ctime>
#include <string>
#include <vector>

namespace i18n {
namespace addressinput {

// Removes the language variation from |language_code|, unless the variation is
// "latn". For example, returns "zh" for "zh-hans". Returns "zh-latn" for
// "zh-latn".
std::string NormalizeLanguageCode(const std::string& language_code);

// Returns |time| serialized into a string.
std::string TimeToString(time_t time);

// Unicode (UTF-8) approximate string comparison. Designed for matching user
// input against canonical data. Returns true for a match.
bool LooseStringCompare(const std::string& a, const std::string& b);

// Splits |str| into a vector of strings delimited by |c|, placing the results
// in |r|. If several instances of |c| are contiguous, or if |str| begins with
// or ends with |c|, then an empty string is inserted.
//
// |str| should not be in a multi-byte encoding like Shift-JIS or GBK in which
// the trailing byte of a multi-byte character can be in the ASCII range.
// UTF-8, and other single/multi-byte ASCII-compatible encodings are OK.
// Note: |c| must be in the ASCII range.
//
// The original source code is from:
// http://src.chromium.org/viewvc/chrome/trunk/src/base/strings/string_split.h?revision=236210
//
// Modifications from original:
//   1) Supports only std::string type.
//   2) Does not trim whitespace.
void SplitString(const std::string& str, char c, std::vector<std::string>* r);

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_UTIL_STRING_UTIL_H_
