// Copyright (C) 2011 Google Inc.
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

// Author: George Yakovlev

#ifndef I18N_PHONENUMBERS_REGEXP_ADAPTER_H_
#define I18N_PHONENUMBERS_REGEXP_ADAPTER_H_

#include <string>

// Regexp adapter to allow pluggable regexp engine, as it is external to
// libphonenumber.

namespace reg_exp {

// The reg exp input class.
// It supports only functions used in phonelibrary.
class RegularExpressionInput {
 public:
  virtual ~RegularExpressionInput() {};
  
  // Matches string to regular expression, returns true if expression was
  // matched, false otherwise, advances position in the match.
  // |reg_exp| - expression to be matched.
  // |beginning_only| - if true match would be successfull only if appears at
  // the beginning of the tested region of the string.
  // |matched_string1| - successfully matched first string. Can be NULL.
  // |matched_string2| - successfully matched second string. Can be NULL.
  virtual bool ConsumeRegExp(std::string const& reg_exp,
                             bool beginning_only,
                             std::string* matched_string1,
                             std::string* matched_string2) = 0;
  // Convert unmatched input to a string.
  virtual std::string ToString() const = 0;
};

// The regular expression class.
// It supports only functions used in phonelibrary.
class RegularExpression {
 public:
  RegularExpression() {}
  virtual ~RegularExpression() {}

  // Matches string to regular expression, returns true if expression was
  // matched, false otherwise, advances position in the match.
  // |input_string| - string to be searched.
  // |beginning_only| - if true match would be successfull only if appears at
  // the beginning of the tested region of the string.
  // |matched_string1| - successfully matched first string. Can be NULL.
  // |matched_string2| - successfully matched second string. Can be NULL.
  // |matched_string3| - successfully matched third string. Can be NULL.
  virtual bool Consume(RegularExpressionInput* input_string,
                       bool beginning_only,
                       std::string* matched_string1 = NULL,
                       std::string* matched_string2 = NULL,
                       std::string* matched_string3 = NULL) const = 0;


  // Matches string to regular expression, returns true if expression was
  // matched, false otherwise.
  // |input_string| - string to be searched.
  // |full_match| - if true match would be successfull only if it matches the
  // complete string.
  // |matched_string| - successfully matched string. Can be NULL.
  virtual bool Match(const char* input_string,
                     bool full_match,
                     std::string* matched_string) const = 0;

  // Replaces match(es) in the |string_to_process|. if |global| is true,
  // replaces all the matches, only the first match otherwise.
  // |replacement_string| - text the matches are replaced with.
  // Returns true if expression successfully processed through the string,
  // even if no actual replacements were made. Returns false in case of an
  // error.
  virtual bool Replace(std::string* string_to_process,
                       bool global,
                       const char* replacement_string) const = 0;
};

RegularExpressionInput* CreateRegularExpressionInput(const char* utf8_input);
RegularExpression* CreateRegularExpression(const char* utf8_regexp);

}  // namespace reg_exp

#endif  // I18N_PHONENUMBERS_REGEXP_ADAPTER_H_
