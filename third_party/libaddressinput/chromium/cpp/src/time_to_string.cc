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

#include "time_to_string.h"

#include <cstdio>
#include <ctime>
#include <string>

#ifdef _MSC_VER
// http://msdn.microsoft.com/en-us/library/2ts7cx93%28v=vs.110%29.aspx
#define snprintf _snprintf
#endif  // _MSC_VER

namespace i18n {
namespace addressinput {

std::string TimeToString(time_t time) {
  char time_string[2 + 3 * sizeof time];
  snprintf(time_string, sizeof time_string, "%ld", time);
  return time_string;
}

}  // namespace addressinput
}  // namespace i18n
