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

#include "rule.h"

#include <libaddressinput/address_field.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include "region_data_constants.h"
#include "util/json.h"
#include "util/string_util.h"

namespace i18n {
namespace addressinput {

namespace {

bool ParseToken(char c, AddressField* field) {
  assert(field != NULL);
  switch (c) {
    case 'R':
      *field = COUNTRY;
      return true;
    case 'S':
      *field = ADMIN_AREA;
      return true;
    case 'C':
      *field = LOCALITY;
      return true;
    case 'D':
      *field = DEPENDENT_LOCALITY;
      return true;
    case 'X':
      *field = SORTING_CODE;
      return true;
    case 'Z':
      *field = POSTAL_CODE;
      return true;
    case 'A':
      *field = STREET_ADDRESS;
      return true;
    case 'N':
      *field = RECIPIENT;
      return true;
    default:
      return false;
  }
}

// Clears |lines|, parses |format|, and adds the address fields and literals to
// |lines|.
//
// For example, the address format in Finland is "%O%n%N%n%A%nAX-%Z %C%nÅLAND".
// It includes the allowed fields prefixed with %, newlines denoted %n, and the
// extra text that should be included on an envelope. It is parsed into:
// {
//     {RECIPIENT},
//     {STREET_ADDRESS},
//     {"AX-", POSTAL_CODE, " ", LOCALITY},
//     {"ÅLAND"}
// }
void ParseAddressFieldsFormat(const std::string& format,
                              std::vector<std::vector<FormatElement> >* lines) {
  assert(lines != NULL);
  lines->clear();
  lines->resize(1);

  std::vector<std::string> format_parts;
  SplitString(format, '%', &format_parts);

  // If the address format starts with a literal, then it will be in the first
  // element of |format_parts|. This literal does not begin with % and should
  // not be parsed as a token.
  if (!format_parts.empty() && !format_parts[0].empty()) {
    lines->back().push_back(FormatElement(format_parts[0]));
  }

  // The rest of the elements in |format_parts| begin with %.
  for (size_t i = 1; i < format_parts.size(); ++i) {
    if (format_parts[i].empty()) {
      continue;
    }

    // The first character after % denotes a field or a newline token.
    const char control_character = format_parts[i][0];

    // The rest of the string after the token is a literal.
    const std::string literal = format_parts[i].substr(1);

    AddressField field = COUNTRY;
    if (ParseToken(control_character, &field)) {
      lines->back().push_back(FormatElement(field));
    } else if (control_character == 'n' && !lines->back().empty()) {
      lines->push_back(std::vector<FormatElement>());
    }

    if (!literal.empty()) {
      lines->back().push_back(FormatElement(literal));
    }
  }
}

// Clears |fields|, parses |required|, and adds the required fields to |fields|.
// For example, parses "SCDX" into {ADMIN_AREA, LOCALITY, DEPENDENT_LOCALITY,
// SORTING_CODE}.
void ParseAddressFieldsRequired(const std::string& required,
                                std::vector<AddressField>* fields) {
  assert(fields != NULL);
  fields->clear();
  for (size_t i = 0; i < required.length(); ++i) {
    AddressField field = COUNTRY;
    if (ParseToken(required[i], &field)) {
      fields->push_back(field);
    }
  }
}

// Finds |target| in |values_to_compare| and sets |selected_value| to the
// associated value from |values_to_select|. Returns true if |target| is in
// |values_to_compare|. |selected_value| should not be NULL. |values_to_compare|
// should not be larger than |values_to_select|.
bool GetMatchingValue(const std::string& target,
                      const std::vector<std::string>& values_to_compare,
                      const std::vector<std::string>& values_to_select,
                      std::string* selected_value) {
  assert(selected_value != NULL);
  assert(values_to_select.size() >= values_to_compare.size());
  for (size_t i = 0; i < values_to_compare.size(); ++i) {
    if (LooseStringCompare(values_to_compare[i], target)) {
      *selected_value = values_to_select[i];
      return true;
    }
  }
  return false;
}

}  // namespace

FormatElement::FormatElement(AddressField field)
    : field(field), literal() {}

FormatElement::FormatElement(const std::string& literal)
    : field(COUNTRY), literal(literal) {
  assert(!literal.empty());
}

FormatElement::~FormatElement() {}

bool FormatElement::operator==(const FormatElement& other) const {
  return field == other.field && literal == other.literal;
}

Rule::Rule() {}

Rule::~Rule() {}

// static
const Rule& Rule::GetDefault() {
  // Allocated once and leaked on shutdown.
  static Rule* default_rule = NULL;
  if (default_rule == NULL) {
    default_rule = new Rule;
    default_rule->ParseSerializedRule(
        RegionDataConstants::GetDefaultRegionData());
  }
  return *default_rule;
}

void Rule::CopyFrom(const Rule& rule) {
  key_ = rule.key_;
  name_ = rule.name_;
  latin_name_ = rule.latin_name_;
  format_ = rule.format_;
  latin_format_ = rule.latin_format_;
  required_ = rule.required_;
  sub_keys_ = rule.sub_keys_;
  languages_ = rule.languages_;
  input_languages_ = rule.input_languages_;
  language_ = rule.language_;
  sub_keys_ = rule.sub_keys_;
  sub_names_ = rule.sub_names_;
  sub_lnames_ = rule.sub_lnames_;
  postal_code_format_ = rule.postal_code_format_;
  admin_area_name_type_ = rule.admin_area_name_type_;
  postal_code_name_type_ = rule.postal_code_name_type_;
}

bool Rule::ParseSerializedRule(const std::string& serialized_rule) {
  scoped_ptr<Json> json(Json::Build());
  if (!json->ParseObject(serialized_rule)) {
    return false;
  }
  ParseJsonRule(*json);
  return true;
}

void Rule::ParseJsonRule(const Json& json_rule) {
  std::string value;
  if (json_rule.GetStringValueForKey("key", &value)) {
    key_.swap(value);
  }

  if (json_rule.GetStringValueForKey("name", &value)) {
    name_.swap(value);
  }

  if (json_rule.GetStringValueForKey("lname", &value)) {
    latin_name_.swap(value);
  }

  if (json_rule.GetStringValueForKey("fmt", &value)) {
    ParseAddressFieldsFormat(value, &format_);
  }

  if (json_rule.GetStringValueForKey("lfmt", &value)) {
    ParseAddressFieldsFormat(value, &latin_format_);
  }

  if (json_rule.GetStringValueForKey("require", &value)) {
    ParseAddressFieldsRequired(value, &required_);
  }

  // Used as a separator in a list of items. For example, the list of supported
  // languages can be "de~fr~it".
  static const char kSeparator = '~';
  if (json_rule.GetStringValueForKey("sub_keys", &value)) {
    SplitString(value, kSeparator, &sub_keys_);
  }

  if (json_rule.GetStringValueForKey("sub_names", &value)) {
    SplitString(value, kSeparator, &sub_names_);
    assert(sub_names_.size() == sub_keys_.size());
  }

  if (json_rule.GetStringValueForKey("sub_lnames", &value)) {
    SplitString(value, kSeparator, &sub_lnames_);
    assert(sub_lnames_.size() == sub_keys_.size());
  }

  if (json_rule.GetStringValueForKey("languages", &value)) {
    SplitString(value, kSeparator, &languages_);
  }

  if (json_rule.GetStringValueForKey("input_languages", &value)) {
    SplitString(value, kSeparator, &input_languages_);
  }

  if (json_rule.GetStringValueForKey("lang", &value)) {
    language_.swap(value);
  }

  if (json_rule.GetStringValueForKey("zip", &value)) {
    postal_code_format_.swap(value);
  }

  if (json_rule.GetStringValueForKey("state_name_type", &value)) {
    admin_area_name_type_.swap(value);
  }

  if (json_rule.GetStringValueForKey("zip_name_type", &value)) {
    postal_code_name_type_.swap(value);
  }
}

const std::string& Rule::GetIdentityField(IdentityField identity_field) const {
  switch (identity_field) {
    case KEY:
      return key_;
    case NAME:
      return name_;
    case LATIN_NAME:
      return latin_name_;
    case IDENTITY_FIELDS_SIZE:
      assert(false);
  }
  return key_;
}

bool Rule::CanonicalizeSubKey(const std::string& user_input,
                              bool keep_input_latin,
                              std::string* sub_key) const {
  assert(sub_key != NULL);

  if (sub_keys_.empty()) {
    *sub_key = user_input;
    return true;
  }

  return GetMatchingValue(user_input, sub_keys_, sub_keys_, sub_key) ||
      GetMatchingValue(user_input, sub_names_, sub_keys_, sub_key) ||
      (keep_input_latin &&
       GetMatchingValue(user_input, sub_lnames_, sub_lnames_, sub_key)) ||
      GetMatchingValue(user_input, sub_lnames_, sub_keys_, sub_key);
}

}  // namespace addressinput
}  // namespace i18n
