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

#include <libaddressinput/address_data.h>

#include <libaddressinput/address_field.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include "region_data_constants.h"
#include "rule.h"
#include "util/string_util.h"

namespace i18n {
namespace addressinput {

namespace {

const std::string* GetMemberForField(const AddressData& address,
                                     AddressField field) {
  switch (field) {
    case COUNTRY:
      return &address.country_code;
    case ADMIN_AREA:
      return &address.administrative_area;
    case LOCALITY:
      return &address.locality;
    case DEPENDENT_LOCALITY:
      return &address.dependent_locality;
    case SORTING_CODE:
      return &address.sorting_code;
    case POSTAL_CODE:
      return &address.postal_code;
    case ORGANIZATION:
      return &address.organization;
    case RECIPIENT:
      return &address.recipient;
    case STREET_ADDRESS:
      break;
  }

  assert(false);
  return NULL;
}

}  // namespace

void AddressData::FormatForDisplay(std::vector<std::string>* lines) const {
  assert(lines != NULL);
  lines->clear();

  Rule rule;
  rule.CopyFrom(Rule::GetDefault());
  rule.ParseSerializedRule(RegionDataConstants::GetRegionData(country_code));

  // If latinized rules are available and the |language_code| of this address is
  // not the primary language code for the region, then use the latinized
  // formatting rules.
  const std::vector<std::vector<FormatElement> >& format =
      rule.GetLatinFormat().empty() ||
      language_code.empty() ||
      NormalizeLanguageCode(language_code) ==
          NormalizeLanguageCode(rule.GetLanguage())
              ? rule.GetFormat() : rule.GetLatinFormat();

  for (size_t i = 0; i < format.size(); ++i) {
    std::string line;
    for (size_t j = 0; j < format[i].size(); ++j) {
      const FormatElement& element = format[i][j];
      if (element.IsField()) {
        if (element.field == STREET_ADDRESS) {
          // Street address field can contain multiple values.
          for (size_t k = 0; k < address_lines.size(); ++k) {
            line += address_lines[k];
            if (k < address_lines.size() - 1) {
              lines->push_back(line);
              line.clear();
            }
          }
        } else {
          line += GetFieldValue(element.field);
        }
      } else {
        line += element.literal;
      }
    }

    if (!line.empty()) {
      lines->push_back(line);
    }
  }
}

const std::string& AddressData::GetFieldValue(AddressField field) const {
  const std::string* field_value = GetMemberForField(*this, field);
  return field_value != NULL ? *field_value : country_code;
}

void AddressData::SetFieldValue(AddressField field, const std::string& value) {
  std::string* field_value =
      const_cast<std::string*>(GetMemberForField(*this, field));
  if (field_value != NULL) {
    *field_value = value;
  }
}

bool AddressData::HasAllRequiredFields() const {
  if (country_code.empty())
    return false;

  Rule rule;
  rule.CopyFrom(Rule::GetDefault());
  if (!rule.ParseSerializedRule(
           RegionDataConstants::GetRegionData(country_code))) {
    return false;
  }

  std::vector< ::i18n::addressinput::AddressField> required_fields =
      rule.GetRequired();
  for (size_t i = 0; i < required_fields.size(); ++i) {
    if (required_fields[i] == STREET_ADDRESS) {
      if (address_lines.empty() || address_lines[0].empty()) {
        return false;
      }
    } else if (GetFieldValue(required_fields[i]).empty()) {
      return false;
    }
  }

  return true;
}

}  // namespace addressinput
}  // namespace i18n
