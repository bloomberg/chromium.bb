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

#ifndef I18N_ADDRESSINPUT_ADDRESS_PROBLEM_H_
#define I18N_ADDRESSINPUT_ADDRESS_PROBLEM_H_

#include <libaddressinput/address_field.h>

#include <iosfwd>

namespace i18n {
namespace addressinput {

// A problem for address validation.
struct AddressProblem {
  // Types of problems encountered in address validation.
  enum Type {
    // The field is empty or whitespace, but it is required for addresses in
    // this country.
    //
    // For example, in the US, administrative area is a required field.
    MISSING_REQUIRED_FIELD,

    // A list of values for the field is defined, but the value does not occur
    // in the list. Applies to hierarchical elements like country code,
    // administrative area, locality, and dependent locality.
    //
    // For example, in the US, the values for for administrative area include
    // "CA", but not "XX".
    UNKNOWN_VALUE,

    // A format for the field is defined, but the value does not match. This is
    // used to match postal code against the general format pattern. Formats
    // indicate how many digits/letters should be present and what punctuation
    // is allowed.
    //
    // For example, in the US, postal codes are five digits with an optional
    // hyphen followed by four digits.
    UNRECOGNIZED_FORMAT,

    // A specific pattern for the field is defined based on a specific
    // sub-region (an administrative area for example), but the value does not
    // match. This is used to match postal code against a regular expression.
    //
    // For example, in the US, postal codes in the state of California start
    // with a '9'.
    MISMATCHING_VALUE
  };

  AddressProblem(AddressField field, Type type, int description_id);
  ~AddressProblem();

  // The address field that has the problem.
  AddressField field;

  // The type of problem.
  Type type;

  // The ID for the string that is the human readable description of the
  // problem.
  int description_id;
};

// Produces human-readable output in logging, for example in unit tests.
// Produces what you would expect for valid values, e.g.
// "MISSING_REQUIRED_FIELD" for MISSING_REQUIRED_FIELD. For invalid values,
// produces "[INVALID]".
std::ostream& operator<<(std::ostream& o, AddressProblem::Type problem_type);

// Produces human-readable output in logging, for example in unit tests.
// Example: [ADMIN_AREA, UNKNOWN_VALUE, "Invalid state"].
std::ostream& operator<<(std::ostream& o, const AddressProblem& problem);

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_ADDRESS_PROBLEM_H_
