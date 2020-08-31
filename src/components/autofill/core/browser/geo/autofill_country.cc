// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/autofill_country.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

// The maximum capacity needed to store a locale up to the country code.
const size_t kLocaleCapacity =
    ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY + 1;

}  // namespace

AutofillCountry::AutofillCountry(const std::string& country_code,
                                 const std::string& locale) {
  CountryDataMap* country_data_map = CountryDataMap::GetInstance();

  // If the country code is an alias (e.g. "GB" for "UK") expand the country
  // code.
  country_code_ = country_data_map->HasCountryCodeAlias(country_code)
                      ? country_data_map->GetCountryCodeForAlias(country_code)
                      : country_code;

  // If there is no entry in the |CountryDataMap| for the
  // |country_code_for_country_data| use the country code  derived from the
  // locale. This reverts to US.
  country_data_map->HasCountryData(country_code_)
      ? country_code_
      : CountryCodeForLocale(locale);

  // Acquire the country address data.
  const CountryData& data = country_data_map->GetCountryData(country_code_);

  // Translate the country name by the supplied local.
  name_ = l10n_util::GetDisplayNameForCountry(country_code_, locale);

  // Get the localized strings associate with the address fields.
  postal_code_label_ = l10n_util::GetStringUTF16(data.postal_code_label_id);
  state_label_ = l10n_util::GetStringUTF16(data.state_label_id);
  address_required_fields_ = data.address_required_fields;
}

AutofillCountry::~AutofillCountry() {}

// static
const std::string AutofillCountry::CountryCodeForLocale(
    const std::string& locale) {
  // Add likely subtags to the locale. In particular, add any likely country
  // subtags -- e.g. for locales like "ru" that only include the language.
  std::string likely_locale;
  UErrorCode error_ignored = U_ZERO_ERROR;
  uloc_addLikelySubtags(locale.c_str(),
                        base::WriteInto(&likely_locale, kLocaleCapacity),
                        kLocaleCapacity, &error_ignored);

  // Extract the country code.
  std::string country_code = icu::Locale(likely_locale.c_str()).getCountry();

  // Default to the United States if we have no better guess.
  if (!base::Contains(CountryDataMap::GetInstance()->country_codes(),
                      country_code)) {
    return "US";
  }

  return country_code;
}

AutofillCountry::AutofillCountry(const std::string& country_code,
                                 const base::string16& name,
                                 const base::string16& postal_code_label,
                                 const base::string16& state_label)
    : country_code_(country_code),
      name_(name),
      postal_code_label_(postal_code_label),
      state_label_(state_label) {}

// Prints a formatted log of a |AutofillCountry| to a |LogBuffer|.
LogBuffer& operator<<(LogBuffer& buffer, const AutofillCountry& country) {
  buffer << LogMessage::kImportAddressProfileFromFormAddressRequirements;
  buffer << Tag{"div"} << Attrib{"class", "country_data"};
  buffer << Tag{"table"};
  buffer << Tr{} << "Country code:" << country.country_code();
  buffer << Tr{} << "Country name:" << country.name();
  buffer << Tr{} << "State required:" << country.requires_state();
  buffer << Tr{} << "Zip required:" << country.requires_zip();
  buffer << Tr{} << "City required:" << country.requires_city();
  buffer << Tr{} << "State label:" << country.state_label();
  buffer << Tr{} << "Postal code label:" << country.postal_code_label();
  buffer << CTag{"table"};
  buffer << CTag{"div"};
  buffer << CTag{};
  return buffer;
}
}  // namespace autofill
