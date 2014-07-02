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

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "grit.h"
#include "grit/libaddressinput_strings.h"
#include "region_data_constants.h"
#include "rule.h"
#include "util/string_util.h"

namespace i18n {
namespace addressinput {

namespace {

int GetMessageIdForField(AddressField field,
                         const std::string& admin_area_name_type,
                         const std::string& postal_code_name_type) {
  switch (field) {
    case COUNTRY:
      return IDS_LIBADDRESSINPUT_I18N_COUNTRY_LABEL;
    case LOCALITY:
      return IDS_LIBADDRESSINPUT_I18N_LOCALITY_LABEL;
    case DEPENDENT_LOCALITY:
      return IDS_LIBADDRESSINPUT_I18N_DEPENDENT_LOCALITY_LABEL;
    case SORTING_CODE:
      return IDS_LIBADDRESSINPUT_I18N_CEDEX_LABEL;
    case STREET_ADDRESS:
      return IDS_LIBADDRESSINPUT_I18N_ADDRESS_LINE1_LABEL;
    case RECIPIENT:
      return IDS_LIBADDRESSINPUT_I18N_RECIPIENT_LABEL;

    case ADMIN_AREA:
      if (admin_area_name_type == "area") {
        return IDS_LIBADDRESSINPUT_I18N_AREA;
      }
      if (admin_area_name_type == "county") {
        return IDS_LIBADDRESSINPUT_I18N_COUNTY_LABEL;
      }
      if (admin_area_name_type == "department") {
        return IDS_LIBADDRESSINPUT_I18N_DEPARTMENT;
      }
      if (admin_area_name_type == "district") {
        return IDS_LIBADDRESSINPUT_I18N_DEPENDENT_LOCALITY_LABEL;
      }
      if (admin_area_name_type == "do_si") {
        return IDS_LIBADDRESSINPUT_I18N_DO_SI;
      }
      if (admin_area_name_type == "emirate") {
        return IDS_LIBADDRESSINPUT_I18N_EMIRATE;
      }
      if (admin_area_name_type == "island") {
        return IDS_LIBADDRESSINPUT_I18N_ISLAND;
      }
      if (admin_area_name_type == "parish") {
        return IDS_LIBADDRESSINPUT_I18N_PARISH;
      }
      if (admin_area_name_type == "prefecture") {
        return IDS_LIBADDRESSINPUT_I18N_PREFECTURE;
      }
      if (admin_area_name_type == "province") {
        return IDS_LIBADDRESSINPUT_I18N_PROVINCE;
      }
      if (admin_area_name_type == "state") {
        return IDS_LIBADDRESSINPUT_I18N_STATE_LABEL;
      }
      break;

    case POSTAL_CODE:
      if (postal_code_name_type == "postal") {
        return IDS_LIBADDRESSINPUT_I18N_POSTAL_CODE_LABEL;
      }
      if (postal_code_name_type == "zip") {
        return IDS_LIBADDRESSINPUT_I18N_ZIP_CODE_LABEL;
      }
      break;
  }

  return INVALID_MESSAGE_ID;
}

// Returns the BCP 47 language code that should be used to format the address
// that the user entered.
//
// If the rule does not contain information about languages, then returns the
// UI language.
//
// If the UI language is either the default language for the country, one of the
// languages for rules, or one of the languages for address input, then returns
// this UI language. If there're no matches, then picks one of the languages in
// the rule and returns it.
//
// If latinized rules are available and the UI language code is not the primary
// language code for this region, then returns the primary language with "-latn"
// appended.
std::string GetComponentsLanguageCode(const Rule& rule,
                                      const std::string& ui_language_code) {
  // Select the default language code for the region.
  std::string default_language_code;
  if (!rule.GetLanguage().empty()) {
    default_language_code = rule.GetLanguage();
  } else if (!rule.GetLanguages().empty()) {
    default_language_code = rule.GetLanguages()[0];
  } else if (!rule.GetInputLanguages().empty()) {
    default_language_code = rule.GetInputLanguages()[0];
  } else {
    // Region does not have any language information (e.g. Antarctica). Use the
    // UI language code as is.
    return ui_language_code;
  }

  // If the UI language code is not set, then use default language code.
  if (ui_language_code.empty()) {
    return default_language_code;
  }

  const std::string& normalized_ui_language_code =
      NormalizeLanguageCode(ui_language_code);
  const std::string& normalized_default_language_code =
      NormalizeLanguageCode(default_language_code);

  // Check whether UI language code matches any language codes in the rule,
  // normalized or as is.
  if (normalized_default_language_code == normalized_ui_language_code ||
      std::find(
          rule.GetLanguages().begin(),
          rule.GetLanguages().end(),
          ui_language_code) != rule.GetLanguages().end() ||
      std::find(
          rule.GetLanguages().begin(),
          rule.GetLanguages().end(),
          normalized_ui_language_code) != rule.GetLanguages().end() ||
      std::find(
          rule.GetInputLanguages().begin(),
          rule.GetInputLanguages().end(),
          ui_language_code) != rule.GetInputLanguages().end() ||
      std::find(
          rule.GetInputLanguages().begin(),
          rule.GetInputLanguages().end(),
          normalized_ui_language_code) != rule.GetInputLanguages().end()) {
    return ui_language_code;
  }

  // The UI language code does not match any language information in the rule.
  return rule.GetLatinFormat().empty()
             ? default_language_code
             : normalized_default_language_code + "-latn";
}

}  // namespace

const std::vector<std::string>& GetRegionCodes() {
  return RegionDataConstants::GetRegionCodes();
}

std::vector<AddressUiComponent> BuildComponents(
    const std::string& region_code,
    const std::string& ui_language_code,
    std::string* components_language_code) {
  std::vector<AddressUiComponent> result;

  Rule rule;
  rule.CopyFrom(Rule::GetDefault());
  if (!rule.ParseSerializedRule(
           RegionDataConstants::GetRegionData(region_code))) {
    return result;
  }

  if (components_language_code != NULL) {
    *components_language_code =
        GetComponentsLanguageCode(rule, ui_language_code);
  }

  // For avoiding showing an input field twice, when the field is displayed
  // twice on an envelope.
  std::set<AddressField> fields;

  // If latinized rules are available and the |ui_language_code| is not the
  // primary language code for the region, then use the latinized formatting
  // rules.
  const std::vector<std::vector<FormatElement> >& format =
      rule.GetLatinFormat().empty() ||
      ui_language_code.empty() ||
      NormalizeLanguageCode(ui_language_code) ==
          NormalizeLanguageCode(rule.GetLanguage())
              ? rule.GetFormat() : rule.GetLatinFormat();

  for (std::vector<std::vector<FormatElement> >::const_iterator
           line_it = format.begin();
       line_it != format.end();
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
                               rule.GetAdminAreaNameType(),
                               rule.GetPostalCodeNameType());
      result.push_back(component);
    }
  }

  return result;
}

const std::string& GetCompactAddressLinesSeparator(
    const std::string& language_code) {
  return RegionDataConstants::GetLanguageCompactLineSeparator(language_code);
}

}  // namespace addressinput
}  // namespace i18n
