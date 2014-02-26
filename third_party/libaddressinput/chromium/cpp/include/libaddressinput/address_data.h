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
//
// An object to store a single address: country code, administrative area,
// locality, etc. The field names correspond to OASIS xAL standard:
// https://www.oasis-open.org/committees/ciq/Downloads/ciq_html_docs.zip

#ifndef I18N_ADDRESSINPUT_ADDRESS_DATA_H_
#define I18N_ADDRESSINPUT_ADDRESS_DATA_H_

#include <libaddressinput/address_field.h>

#include <string>
#include <vector>

namespace i18n {
namespace addressinput {

// Stores an address. Sample usage:
//    AddressData address;
//    address.recipient = "Chen-Kang Yang";
//    address.organization = "Google";
//    address.address_lines.push_back("1098 Alta Ave");
//    address.administrative_area = "CA";
//    address.locality = "Mountain View";
//    address.postal_code = "94043";
//    address.country_code = "US";
//    address.language_code = "en";
//    Process(address);
struct AddressData {
  // Clears |lines| and populates it with the lines of the address as they
  // should appear on an envelope for |country_code|. The |lines| parameter
  // should not be NULL.
  //
  // If there're no address formatting rules for |country_code|, then the
  // default rules are used:
  // https://i18napis.appspot.com/ssl-address/data/ZZ
  void FormatForDisplay(std::vector<std::string>* lines) const;

  // Returns the value of the |field|. The parameter should not be
  // STREET_ADDRESS, which comprises multiple fields.
  const std::string& GetFieldValue(AddressField field) const;

  // Sets the |field| to |value|. The parameter should not be STREET_ADDRESS,
  // which comprises multiple fields.
  void SetFieldValue(AddressField field, const std::string& value);

  // Guesses the BCP 47 language code to be used for formatting the address for
  // display. For example, guesses "en" if the |country_code| is "US". Can
  // return an empty string.
  const std::string& GuessLanguageCode() const;

  // The BCP 47 language code used to guide how the address is formatted for
  // display. The same address may have different representations in different
  // languages.
  // For example, the French name of "New Mexico" is "Nouveau-Mexique".
  std::string language_code;

  // The uppercase CLDR country/region code.
  // For example, "US" for United States.
  // (Note: Use "GB", not "UK", for Great Britain.)
  std::string country_code;

  // Top-level administrative subdivision of this country.
  // Examples: US state, IT region, UK constituent nation, JP prefecture.
  std::string administrative_area;

  // Generally refers to the city/town portion of an address.
  // Examples: US city, IT comune, UK post town.
  std::string locality;

  // Dependent locality or sublocality. Used for UK dependent localities, or
  // neighborhoods or boroughs in other locations.
  std::string dependent_locality;

  // Identifies recipients of large volumes of mail. Used in only a few
  // countries.
  // Examples: FR CEDEX.
  std::string sorting_code;

  // The alphanumeric value generally assigned to geographical areas, but
  // sometimes also assigned to individual addresses.
  // Examples: "94043", "94043-1351", "SW1W", "SW1W 9TQ".
  std::string postal_code;

  // The free format street address lines.
  std::vector<std::string> address_lines;

  // The firm, company, or organization.
  std::string organization;

  // The name of the recipient or contact person. Not present in xAL.
  std::string recipient;
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_ADDRESS_DATA_H_
