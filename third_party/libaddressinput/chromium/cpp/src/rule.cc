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

#include "grit.h"
#include "grit/libaddressinput_strings.h"
#include "region_data_constants.h"
#include "util/json.h"
#include "util/string_split.h"

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
    case 'O':
      *field = ORGANIZATION;
      return true;
    case 'N':
      *field = RECIPIENT;
      return true;
    default:
      return false;
  }
}

// Clears |fields|, parses |format|, and adds the format address fields to
// |fields|.
//
// For example, the address format in Switzerland is "%O%n%N%n%A%nAX-%Z
// %C%nÅLAND". It includes the allowed fields prefixed with %, newlines denoted
// %n, and the extra text that should be included on an envelope. This function
// parses only the tokens denoted % to determine how an address input form
// should be laid out.
//
// The format string "%O%n%N%n%A%nAX-%Z%C%nÅLAND" is parsed into
// {{ORGANIZATION}, {RECIPIENT}, {STREET_ADDRESS}, {POSTAL_CODE, LOCALITY}, {}}.
void ParseAddressFieldsFormat(const std::string& format,
                              std::vector<std::vector<AddressField> >* fields) {
  assert(fields != NULL);
  fields->clear();
  fields->resize(1);
  std::vector<std::string> format_parts;
  SplitString(format, '%', &format_parts);
  for (size_t i = 1; i < format_parts.size(); ++i) {
    AddressField field = COUNTRY;
    if (ParseToken(format_parts[i][0], &field)) {
      fields->back().push_back(field);
    } else if (format_parts[i][0] == 'n') {
      fields->push_back(std::vector<AddressField>());
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

int GetAdminAreaMessageId(const std::string& admin_area_type, bool error) {
  if (admin_area_type == "area") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_AREA
                 : IDS_LIBADDRESSINPUT_I18N_AREA;
  }
  if (admin_area_type == "county") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_COUNTY_LABEL
                 : IDS_LIBADDRESSINPUT_I18N_COUNTY_LABEL;
  }
  if (admin_area_type == "department") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_DEPARTMENT
                 : IDS_LIBADDRESSINPUT_I18N_DEPARTMENT;
  }
  if (admin_area_type == "district") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_DEPENDENT_LOCALITY_LABEL
                 : IDS_LIBADDRESSINPUT_I18N_DEPENDENT_LOCALITY_LABEL;
  }
  if (admin_area_type == "do_si") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_DO_SI
                 : IDS_LIBADDRESSINPUT_I18N_DO_SI;
  }
  if (admin_area_type == "emirate") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_EMIRATE
                 : IDS_LIBADDRESSINPUT_I18N_EMIRATE;
  }
  if (admin_area_type == "island") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_ISLAND
                 : IDS_LIBADDRESSINPUT_I18N_ISLAND;
  }
  if (admin_area_type == "parish") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_PARISH
                 : IDS_LIBADDRESSINPUT_I18N_PARISH;
  }
  if (admin_area_type == "prefecture") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_PREFECTURE
                 : IDS_LIBADDRESSINPUT_I18N_PREFECTURE;
  }
  if (admin_area_type == "province") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_PROVINCE
                 : IDS_LIBADDRESSINPUT_I18N_PROVINCE;
  }
  if (admin_area_type == "state") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_STATE_LABEL
                 : IDS_LIBADDRESSINPUT_I18N_STATE_LABEL;
  }
  return INVALID_MESSAGE_ID;
}

int GetPostalCodeMessageId(const std::string& postal_code_type, bool error) {
  if (postal_code_type == "postal") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_POSTAL_CODE_LABEL
                 : IDS_LIBADDRESSINPUT_I18N_POSTAL_CODE_LABEL;
  }
  if (postal_code_type == "zip") {
    return error ? IDS_LIBADDRESSINPUT_I18N_INVALID_ZIP_CODE_LABEL
                 : IDS_LIBADDRESSINPUT_I18N_ZIP_CODE_LABEL;
  }
  return INVALID_MESSAGE_ID;
}

}  // namespace

Rule::Rule()
    : format_(),
      required_(),
      sub_keys_(),
      languages_(),
      language_(),
      postal_code_format_(),
      admin_area_name_message_id_(INVALID_MESSAGE_ID),
      postal_code_name_message_id_(INVALID_MESSAGE_ID) {}

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
  format_ = rule.format_;
  required_ = rule.required_;
  sub_keys_ = rule.sub_keys_;
  languages_ = rule.languages_;
  language_ = rule.language_;
  postal_code_format_ = rule.postal_code_format_;
  admin_area_name_message_id_ = rule.admin_area_name_message_id_;
  invalid_admin_area_message_id_ = rule.invalid_admin_area_message_id_;
  postal_code_name_message_id_ = rule.postal_code_name_message_id_;
  invalid_postal_code_message_id_ = rule.invalid_postal_code_message_id_;
}

bool Rule::ParseSerializedRule(const std::string& serialized_rule) {
  scoped_ptr<Json> json(Json::Build());
  if (!json->ParseObject(serialized_rule)) {
    return false;
  }

  std::string value;
  if (json->GetStringValueForKey("fmt", &value)) {
    ParseAddressFieldsFormat(value, &format_);
  }

  if (json->GetStringValueForKey("require", &value)) {
    ParseAddressFieldsRequired(value, &required_);
  }

  // Used as a separator in a list of items. For example, the list of supported
  // languages can be "de~fr~it".
  static const char kSeparator = '~';
  if (json->GetStringValueForKey("sub_keys", &value)) {
    SplitString(value, kSeparator, &sub_keys_);
  }

  if (json->GetStringValueForKey("languages", &value)) {
    SplitString(value, kSeparator, &languages_);
  }

  if (json->GetStringValueForKey("lang", &value)) {
    language_ = value;
  }

  if (json->GetStringValueForKey("zip", &value)) {
    postal_code_format_ = value;
  }

  if (json->GetStringValueForKey("state_name_type", &value)) {
    admin_area_name_message_id_ = GetAdminAreaMessageId(value, false);
    invalid_admin_area_message_id_ = GetAdminAreaMessageId(value, true);
  }

  if (json->GetStringValueForKey("zip_name_type", &value)) {
    postal_code_name_message_id_ = GetPostalCodeMessageId(value, false);
    invalid_postal_code_message_id_ = GetPostalCodeMessageId(value, true);
  }

  return true;
}

int Rule::GetInvalidFieldMessageId(AddressField field) const {
  switch (field) {
    case ADMIN_AREA:
      return invalid_admin_area_message_id_;
    case LOCALITY:
      return IDS_LIBADDRESSINPUT_I18N_INVALID_LOCALITY_LABEL;
    case DEPENDENT_LOCALITY:
      return IDS_LIBADDRESSINPUT_I18N_INVALID_DEPENDENT_LOCALITY_LABEL;
    case POSTAL_CODE:
      return invalid_postal_code_message_id_;
    default:
      return IDS_LIBADDRESSINPUT_I18N_INVALID_ENTRY;
  }
}

}  // namespace addressinput
}  // namespace i18n
