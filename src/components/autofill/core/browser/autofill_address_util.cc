// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_address_util.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::AutofillCountry;
using autofill::ServerFieldType;
using i18n::addressinput::AddressField;
using i18n::addressinput::AddressUiComponent;
using i18n::addressinput::Localization;

namespace autofill {

namespace {

// Returns a vector of AddressUiComponent for `country_code` when using
// `ui_language_code`. If no components are available for `country_code`, it
// defaults back to the US. If `ui_language_code` is not valid,  the default
// format is returned.
std::vector<AddressUiComponent> GetAddressComponents(
    const std::string& country_code,
    const std::string& ui_language_code,
    std::string* components_language_code) {
  DCHECK(components_language_code);

  // Return strings in the current application locale.
  Localization localization;
  localization.SetGetter(l10n_util::GetStringUTF8);
  for (const auto& country : {country_code, std::string("US")}) {
    std::vector<AddressUiComponent> components =
        ::i18n::addressinput::BuildComponentsWithLiterals(
            country, localization, ui_language_code, components_language_code);
    if (!components.empty())
      return components;
  }
  NOTREACHED();
  return {};
}

}  // namespace

ServerFieldType AddressFieldToServerFieldType(AddressField address_field) {
  switch (address_field) {
    case ::i18n::addressinput::COUNTRY:
      return ADDRESS_HOME_COUNTRY;
    case ::i18n::addressinput::ADMIN_AREA:
      return ADDRESS_HOME_STATE;
    case ::i18n::addressinput::LOCALITY:
      return ADDRESS_HOME_CITY;
    case ::i18n::addressinput::DEPENDENT_LOCALITY:
      return ADDRESS_HOME_DEPENDENT_LOCALITY;
    case ::i18n::addressinput::SORTING_CODE:
      return ADDRESS_HOME_SORTING_CODE;
    case ::i18n::addressinput::POSTAL_CODE:
      return ADDRESS_HOME_ZIP;
    case ::i18n::addressinput::STREET_ADDRESS:
      return ADDRESS_HOME_STREET_ADDRESS;
    case ::i18n::addressinput::ORGANIZATION:
      return COMPANY_NAME;
    case ::i18n::addressinput::RECIPIENT:
      return NAME_FULL;
  }
}

void GetAddressComponents(
    const std::string& country_code,
    const std::string& ui_language_code,
    bool include_literals,
    std::vector<std::vector<AddressUiComponent>>* address_components,
    std::string* components_language_code) {
  std::string not_used;
  std::vector<AddressUiComponent> components = GetAddressComponents(
      country_code, ui_language_code,
      components_language_code ? components_language_code : &not_used);
  std::vector<AddressUiComponent>* line_components = nullptr;
  for (const AddressUiComponent& component : components) {
    // Start a new line if this is the first line, or a new line literal exists.
    if (!line_components || component.literal == "\n") {
      address_components->push_back(std::vector<AddressUiComponent>());
      line_components = &address_components->back();
    }

    if (!component.literal.empty()) {
      if (!include_literals)
        continue;
      // No need to return new line literals since components are split into
      // different lines anyway (one line per vector).
      if (component.literal == "\n")
        continue;
    }

    line_components->push_back(component);
  }
  // Filter empty lines. Those can appear e.g. when the line consists only of
  // literals and |include_literals| is false.
  address_components->erase(
      base::ranges::remove_if(*address_components,
                              [](auto line) { return line.empty(); }),
      address_components->end());
}

std::u16string GetEnvelopeStyleAddress(const AutofillProfile& profile,
                                       const std::string& ui_language_code,
                                       bool include_recipient,
                                       bool include_country) {
  const AutofillType kCountryCode(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  const std::u16string& country_code =
      profile.GetInfo(kCountryCode, ui_language_code);

  std::string not_used;
  std::vector<AddressUiComponent> components = GetAddressComponents(
      base::UTF16ToUTF8(country_code), ui_language_code, &not_used);

  DCHECK(!components.empty());
  std::string address;
  for (const AddressUiComponent& component : components) {
    // Add string literals directly.
    if (!component.literal.empty()) {
      address += component.literal;
      continue;
    }
    if (!include_recipient &&
        component.field == ::i18n::addressinput::RECIPIENT) {
      continue;
    }
    ServerFieldType type =
        autofill::AddressFieldToServerFieldType(component.field);
    if (type == NAME_FULL)
      type = NAME_FULL_WITH_HONORIFIC_PREFIX;
    address += base::UTF16ToUTF8(profile.GetInfo(type, ui_language_code));
  }
  if (include_country) {
    address += "\n";
    address += base::UTF16ToUTF8(
        profile.GetInfo(ADDRESS_HOME_COUNTRY, ui_language_code));
  }

  // Remove all white spaces and new lines from the beginning and the end of the
  // address.
  base::TrimString(address, base::kWhitespaceASCII, &address);

  // Collapse new lines to remove empty lines.
  re2::RE2::GlobalReplace(&address, re2::RE2("\\n+"), "\n");

  // Collapse white spaces.
  re2::RE2::GlobalReplace(&address, re2::RE2("[ ]+"), " ");

  return base::UTF8ToUTF16(address);
}

std::u16string GetProfileDescription(const AutofillProfile& profile,
                                     const std::string& ui_language_code,
                                     bool include_address_and_contacts) {
  // All user-visible fields.
  static constexpr ServerFieldType kDetailsFields[] = {
      NAME_FULL,
      ADDRESS_HOME_LINE1,
      ADDRESS_HOME_LINE2,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER,
      COMPANY_NAME,
      ADDRESS_HOME_COUNTRY};

  if (!include_address_and_contacts) {
    return profile.GetInfo(NAME_FULL, ui_language_code);
  }

  return profile.ConstructInferredLabel(
      kDetailsFields, base::size(kDetailsFields),
      /*num_fields_to_include=*/2, ui_language_code);
}

std::vector<ProfileValueDifference> GetProfileDifferenceForUi(
    const AutofillProfile& first_profile,
    const AutofillProfile& second_profile,
    const std::string& app_locale) {
  static constexpr ServerFieldType kTypeToCompare[] = {
      NAME_FULL_WITH_HONORIFIC_PREFIX, ADDRESS_HOME_ADDRESS, EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER};

  base::flat_map<ServerFieldType, std::pair<std::u16string, std::u16string>>
      differences = AutofillProfileComparator::GetProfileDifferenceMap(
          first_profile, second_profile,
          autofill::ServerFieldTypeSet(std::begin(kTypeToCompare),
                                       std::end(kTypeToCompare)),
          app_locale);

  std::u16string first_address = GetEnvelopeStyleAddress(
      first_profile, app_locale, /*include_recipient=*/false,
      /*include_country=*/true);
  std::u16string second_address = GetEnvelopeStyleAddress(
      second_profile, app_locale, /*include_recipient=*/false,
      /*include_country=*/true);

  std::vector<ProfileValueDifference> differences_for_ui;
  for (ServerFieldType type : kTypeToCompare) {
    // Address is handled seprately.
    if (type == ADDRESS_HOME_ADDRESS) {
      if (first_address != second_address) {
        differences_for_ui.emplace_back(
            ProfileValueDifference{type, first_address, second_address});
      }
      continue;
    }
    auto it = differences.find(type);
    if (it == differences.end())
      continue;
    differences_for_ui.emplace_back(
        ProfileValueDifference{type, it->second.first, it->second.second});
  }
  return differences_for_ui;
}

}  // namespace autofill
