// Copyright (C) 2013 Google Inc.
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

#include <libaddressinput/address_problem.h>

#include <ostream>

namespace i18n {
namespace addressinput {

std::ostream& operator<<(std::ostream& o, AddressProblem::Type problem_type) {
  switch (problem_type) {
    case AddressProblem::MISSING_REQUIRED_FIELD:
      o << "MISSING_REQUIRED_FIELD";
      break;
    case AddressProblem::UNKNOWN_VALUE:
      o << "UNKNOWN_VALUE";
      break;
    case AddressProblem::UNRECOGNIZED_FORMAT:
      o << "UNRECOGNIZED_FORMAT";
      break;
    case AddressProblem::MISMATCHING_VALUE:
      o << "MISMATCHING_VALUE";
      break;
    default :
      o << "[INVALID]";
      break;
  }
  return o;
}

std::ostream& operator<<(std::ostream& o, const AddressProblem& problem) {
  o << "[" << problem.field << ", "
           << problem.type << ", \""
           << problem.description << "\"]";
  return o;
}

}  // namespace addressinput
}  // namespace i18n
