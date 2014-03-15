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

#ifndef I18N_ADDRESSINPUT_UTIL_CANONICALIZE_STRING_H_
#define I18N_ADDRESSINPUT_UTIL_CANONICALIZE_STRING_H_

#include <libaddressinput/util/scoped_ptr.h>

#include <string>

namespace i18n {
namespace addressinput {

class StringCanonicalizer {
 public:
  static scoped_ptr<StringCanonicalizer> Build();

  virtual ~StringCanonicalizer() {}

  // Returns |original| string modified to enable loose comparisons and sorting.
  virtual std::string CanonicalizeString(const std::string& original) = 0;
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_UTIL_CANONICALIZE_STRING_H_
