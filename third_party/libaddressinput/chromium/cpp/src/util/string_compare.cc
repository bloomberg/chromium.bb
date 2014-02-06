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

#include "string_compare.h"

namespace i18n {
namespace addressinput {

bool LooseStringCompare(const std::string& a, const std::string& b) {
  // The best we can do without ICU.
  return a == b;
}

}  // namespace addressinput
}  // namespace i18n
