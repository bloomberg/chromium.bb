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

#include <libaddressinput/address_ui.h>

#include <libaddressinput/address_field.h>
#include <libaddressinput/address_ui_component.h>

#include <set>
#include <string>
#include <vector>

#include "grit.h"
#include "grit/libaddressinput_strings.h"
#include "region_data_constants.h"
#include "rule.h"

namespace i18n {
namespace addressinput {

namespace {

int GetMessageIdForField(AddressField field,
                         int admin_area_name_message_id,
                         int postal_code_name_message_id) {
  switch (field) {
    case COUNTRY:
      return IDS_LIBADDRESSINPUT_I18N_COUNTRY_LABEL;
    case ADMIN_AREA:
      return admin_area_name_message_id;
    case LOCALITY:
      return IDS_LIBADDRESSINPUT_I18N_LOCALITY_LABEL;
    case DEPENDENT_LOCALITY:
      return IDS_LIBADDRESSINPUT_I18N_DEPENDENT_LOCALITY_LABEL;
    case SORTING_CODE:
      return IDS_LIBADDRESSINPUT_I18N_CEDEX_LABEL;
    case POSTAL_CODE:
      return postal_code_name_message_id;
    case STREET_ADDRESS:
      return IDS_LIBADDRESSINPUT_I18N_ADDRESS_LINE1_LABEL;
    case ORGANIZATION:
      return IDS_LIBADDRESSINPUT_I18N_ORGANIZATION_LABEL;
    case RECIPIENT:
      return IDS_LIBADDRESSINPUT_I18N_RECIPIENT_LABEL;
    default:
      return INVALID_MESSAGE_ID;
  }
}

}  // namespace

const std::vector<std::string>& GetRegionCodes() {
  return RegionDataConstants::GetRegionCodes();
}

std::vector<AddressUiComponent> BuildComponents(
    const std::string& region_code) {
  std::vector<AddressUiComponent> result;

  Rule rule;
  rule.CopyFrom(Rule::GetDefault());
  if (!rule.ParseSerializedRule(
           RegionDataConstants::GetRegionData(region_code))) {
    return result;
  }

  // For avoiding showing an input field twice, when the field is displayed
  // twice on an envelope.
  std::set<AddressField> fields;

  for (std::vector<std::vector<FormatElement> >::const_iterator
           line_it = rule.GetFormat().begin();
       line_it != rule.GetFormat().end();
       ++line_it) {
    int num_fields_this_row = 0;
    for (std::vector<FormatElement>::const_iterator element_it =
             line_it->begin();
         element_it != line_it->end();
         ++element_it) {
      if (element_it->IsField()) {
        ++num_fields_this_row;
      }
    }

    for (std::vector<FormatElement>::const_iterator element_it =
             line_it->begin();
         element_it != line_it->end();
         ++element_it) {
      AddressField field = element_it->field;
      if (!element_it->IsField() || fields.find(field) != fields.end()) {
        continue;
      }
      fields.insert(field);

      AddressUiComponent component;
      component.length_hint =
          num_fields_this_row == 1 ? AddressUiComponent::HINT_LONG
                                   : AddressUiComponent::HINT_SHORT;
      component.field = field;
      component.name_id =
          GetMessageIdForField(field,
                               rule.GetAdminAreaNameMessageId(),
                               rule.GetPostalCodeNameMessageId());
      result.push_back(component);
    }
  }

  return result;
}

std::vector<AddressField> GetRequiredFields(const std::string& region_code) {
  Rule rule;
  rule.CopyFrom(Rule::GetDefault());
  if (!rule.ParseSerializedRule(
           RegionDataConstants::GetRegionData(region_code))) {
    return std::vector<AddressField>();
  }

  return rule.GetRequired();
}

const std::string& GetCompactAddressLinesSeparator(
    const std::string& language_code) {
  return RegionDataConstants::GetLanguageCompactLineSeparator(language_code);
}

}  // namespace addressinput
}  // namespace i18n
