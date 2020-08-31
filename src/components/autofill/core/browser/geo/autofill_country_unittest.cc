// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#if defined(ANDROID)
#include "base/android/build_info.h"
#endif

using autofill::CountryDataMap;
using base::ASCIIToUTF16;

namespace autofill {

// Test the constructor and accessors
TEST(AutofillCountryTest, AutofillCountry) {
  AutofillCountry united_states_en("US", "en_US");
  EXPECT_EQ("US", united_states_en.country_code());
  EXPECT_EQ(ASCIIToUTF16("United States"), united_states_en.name());
  EXPECT_EQ(ASCIIToUTF16("ZIP code"), united_states_en.postal_code_label());
  EXPECT_EQ(ASCIIToUTF16("State"), united_states_en.state_label());

  AutofillCountry united_states_es("US", "es");
  EXPECT_EQ("US", united_states_es.country_code());
  EXPECT_EQ(ASCIIToUTF16("Estados Unidos"), united_states_es.name());

  AutofillCountry great_britain_uk_alias("UK", "en_GB");
  EXPECT_EQ("GB", great_britain_uk_alias.country_code());
  EXPECT_EQ("GB", great_britain_uk_alias.country_code());
  EXPECT_EQ(ASCIIToUTF16("United Kingdom"), great_britain_uk_alias.name());

  AutofillCountry canada_en("CA", "en_US");
  EXPECT_EQ("CA", canada_en.country_code());
  EXPECT_EQ(ASCIIToUTF16("Canada"), canada_en.name());
  EXPECT_EQ(ASCIIToUTF16("Postal code"), canada_en.postal_code_label());
  EXPECT_EQ(ASCIIToUTF16("Province"), canada_en.state_label());

  AutofillCountry canada_hu("CA", "hu");
  EXPECT_EQ("CA", canada_hu.country_code());
  EXPECT_EQ(ASCIIToUTF16("Kanada"), canada_hu.name());
}

// Test locale to country code mapping.
TEST(AutofillCountryTest, CountryCodeForLocale) {
  EXPECT_EQ("US", AutofillCountry::CountryCodeForLocale("en_US"));
  EXPECT_EQ("CA", AutofillCountry::CountryCodeForLocale("fr_CA"));
  EXPECT_EQ("FR", AutofillCountry::CountryCodeForLocale("fr"));
  EXPECT_EQ("US", AutofillCountry::CountryCodeForLocale("Unknown"));
  // "es-419" isn't associated with a country. See base/l10n/l10n_util.cc
  // for details about this locale. Default to US.
  EXPECT_EQ("US", AutofillCountry::CountryCodeForLocale("es-419"));
}

// Test the address requirement methods for the US.
TEST(AutofillCountryTest, UsaAddressRequirements) {
  // The US requires a zip, state, city and line1 entry.
  AutofillCountry us_autofill_country("US", "en_US");

  EXPECT_FALSE(us_autofill_country.requires_zip_or_state());
  EXPECT_TRUE(us_autofill_country.requires_zip());
  EXPECT_TRUE(us_autofill_country.requires_state());
  EXPECT_TRUE(us_autofill_country.requires_city());
  EXPECT_TRUE(us_autofill_country.requires_line1());
}

// Test the address requirement method for Brazil.
TEST(AutofillCountryTest, BrAddressRequirements) {
  // Brazil only requires a zip entry.
  AutofillCountry brazil_autofill_country("BR", "en_US");

  EXPECT_FALSE(brazil_autofill_country.requires_zip_or_state());
  EXPECT_TRUE(brazil_autofill_country.requires_zip());
  EXPECT_TRUE(brazil_autofill_country.requires_state());
  EXPECT_TRUE(brazil_autofill_country.requires_city());
  EXPECT_TRUE(brazil_autofill_country.requires_line1());
}

// Test the address requirement method for Turkey.
TEST(AutofillCountryTest, TrAddressRequirements) {
  // Brazil only requires a zip entry.
  AutofillCountry turkey_autofill_country("TR", "en_US");

  // Although ZIP codes are existing in Turkey, they are commonly used.
  EXPECT_FALSE(turkey_autofill_country.requires_zip());
  // In Turkey, a district is the largest level of the address hierarchy and
  // mapped to the Autofill state.
  EXPECT_TRUE(turkey_autofill_country.requires_state());
  // And the province as the second largest level is mapped to city.
  EXPECT_TRUE(turkey_autofill_country.requires_city());
  EXPECT_TRUE(turkey_autofill_country.requires_line1());
}

// Test mapping all country codes to country names.
TEST(AutofillCountryTest, AllCountryCodesHaveCountryName) {
  std::set<std::string> expected_failures;
#if defined(ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_KITKAT) {
    expected_failures.insert("BQ");
    expected_failures.insert("SS");
    expected_failures.insert("XK");
  }
#endif
  const std::vector<std::string>& country_codes =
      CountryDataMap::GetInstance()->country_codes();
  for (const std::string& country_code : country_codes) {
    if (base::Contains(expected_failures, country_code))
      continue;
    SCOPED_TRACE("Country code '" + country_code + "' should have a name.");
    EXPECT_NE(ASCIIToUTF16(country_code),
              AutofillCountry(country_code, "en").name());
  }
}

// Test alias mappings for falsely existing country codes.
TEST(AutofillCountryTest, AliasMappingsForCountryData) {
  CountryDataMap* country_data_map = CountryDataMap::GetInstance();

  // There should be country data for the "GB".
  EXPECT_TRUE(country_data_map->HasCountryData("GB"));

  // Check the correctness of the alias definitions.
  EXPECT_TRUE(country_data_map->HasCountryCodeAlias("UK"));
  EXPECT_FALSE(country_data_map->HasCountryCodeAlias("does_not_exist"));

  // Query not existing mapping.
  auto expected_country_code = std::string();
  auto actual_country_code =
      country_data_map->GetCountryCodeForAlias("does_not_exist");
  EXPECT_EQ(expected_country_code, actual_country_code);

  // GB should map the UK.
  expected_country_code = "GB";
  actual_country_code = country_data_map->GetCountryCodeForAlias("UK");
  EXPECT_EQ(expected_country_code, actual_country_code);
}

}  // namespace autofill
