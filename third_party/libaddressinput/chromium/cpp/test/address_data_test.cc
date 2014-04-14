// Copyright (C) 2014 Google Inc.
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

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

using i18n::addressinput::AddressData;

TEST(AddressDataTest, FormatForDisplayEmpty) {
  AddressData address;
  std::vector<std::string> actual;
  address.FormatForDisplay(&actual);
  EXPECT_EQ(std::vector<std::string>(), actual);
}

TEST(AddressDataTest, FormatForDisplayUs) {
  AddressData address;
  address.country_code = "US";
  address.administrative_area = "Texas";
  address.locality = "Houston";
  address.postal_code = "77005";
  address.address_lines.push_back("123 Main St");
  address.address_lines.push_back("Apt 2");
  address.organization = "ACME Corp.";
  address.recipient = "John Doe";

  std::vector<std::string> actual;
  address.FormatForDisplay(&actual);

  std::vector<std::string> expected;
  expected.push_back(address.recipient);
  expected.push_back(address.organization);
  expected.insert(expected.end(),
                  address.address_lines.begin(),
                  address.address_lines.end());
  expected.push_back(address.locality + ", " + address.administrative_area +
                     " " + address.postal_code);

  EXPECT_EQ(expected, actual);
}

TEST(AddressDataTest, FormatForDisplayAr) {
  AddressData address;
  address.country_code = "AR";
  address.administrative_area = "Capital Federal";
  address.locality = "Buenos Aires";
  address.postal_code = "C1001AFB";
  address.address_lines.push_back("Su Calle 123");
  address.address_lines.push_back("5° Piso");
  address.organization = "Empresa Ejemplo";
  address.recipient = "Juan Perez";

  std::vector<std::string> actual;
  address.FormatForDisplay(&actual);

  std::vector<std::string> expected;
  expected.push_back(address.recipient);
  expected.push_back(address.organization);
  expected.insert(expected.end(),
                  address.address_lines.begin(),
                  address.address_lines.end());
  expected.push_back(address.postal_code + " " + address.locality);
  expected.push_back(address.administrative_area);

  EXPECT_EQ(expected, actual);
}

TEST(AddressDataTest, FormatForDisplayJp) {
  AddressData address;
  address.country_code = "JP";
  address.language_code = "ja";
  address.administrative_area = "東京都";
  address.locality = "渋谷区";
  address.postal_code = "150-8512";
  address.address_lines.push_back("桜丘町26-1");
  address.address_lines.push_back("セルリアンタワー6階");
  address.organization = "グーグル株式会社";
  address.recipient = "村上 美紀";

  std::vector<std::string> actual;
  address.FormatForDisplay(&actual);

  std::vector<std::string> expected;
  expected.push_back("〒" + address.postal_code);
  expected.push_back(address.administrative_area + address.locality);
  expected.insert(expected.end(),
                  address.address_lines.begin(),
                  address.address_lines.end());
  expected.push_back(address.organization);
  expected.push_back(address.recipient);

  EXPECT_EQ(expected, actual);
}

TEST(AddressDataTest, FormatForDisplayJpLatn) {
  AddressData address;
  address.country_code = "JP";
  address.language_code = "ja-latn";
  address.administrative_area = "Tokyo";
  address.locality = "Shibuya-ku";
  address.postal_code = "150-8512";
  address.address_lines.push_back("26-1 Sakuragaoka-cho");
  address.address_lines.push_back("Cerulean Tower 6F");
  address.organization = "Google Japan Inc.";
  address.recipient = "Miki Murakami";

  std::vector<std::string> actual;
  address.FormatForDisplay(&actual);

  std::vector<std::string> expected;
  expected.push_back(address.recipient);
  expected.push_back(address.organization);
  expected.insert(expected.end(),
                  address.address_lines.begin(),
                  address.address_lines.end());
  expected.push_back(address.locality + ", "+ address.administrative_area);
  expected.push_back(address.postal_code);

  EXPECT_EQ(expected, actual);
}

TEST(AddressDataTest, FormatForDisplayJpLatnCapitalized) {
  AddressData address;
  address.country_code = "JP";
  address.language_code = "ja-Latn";
  address.administrative_area = "Tokyo";
  address.locality = "Shibuya-ku";
  address.postal_code = "150-8512";
  address.address_lines.push_back("26-1 Sakuragaoka-cho");
  address.address_lines.push_back("Cerulean Tower 6F");
  address.organization = "Google Japan Inc.";
  address.recipient = "Miki Murakami";

  std::vector<std::string> actual;
  address.FormatForDisplay(&actual);

  std::vector<std::string> expected;
  expected.push_back(address.recipient);
  expected.push_back(address.organization);
  expected.insert(expected.end(),
                  address.address_lines.begin(),
                  address.address_lines.end());
  expected.push_back(address.locality + ", "+ address.administrative_area);
  expected.push_back(address.postal_code);

  EXPECT_EQ(expected, actual);
}

TEST(AddressDataTest, FormatForDisplayJpLatnUs) {
  AddressData address;
  address.country_code = "JP";
  address.language_code = "ja-latn-US";
  address.administrative_area = "Tokyo";
  address.locality = "Shibuya-ku";
  address.postal_code = "150-8512";
  address.address_lines.push_back("26-1 Sakuragaoka-cho");
  address.address_lines.push_back("Cerulean Tower 6F");
  address.organization = "Google Japan Inc.";
  address.recipient = "Miki Murakami";

  std::vector<std::string> actual;
  address.FormatForDisplay(&actual);

  std::vector<std::string> expected;
  expected.push_back(address.recipient);
  expected.push_back(address.organization);
  expected.insert(expected.end(),
                  address.address_lines.begin(),
                  address.address_lines.end());
  expected.push_back(address.locality + ", "+ address.administrative_area);
  expected.push_back(address.postal_code);

  EXPECT_EQ(expected, actual);
}

TEST(AddressDataTest, FormatForDisplayWithStreetCi) {
  AddressData address;
  address.country_code = "CI";
  address.address_lines.push_back("Street Line 1");
  address.locality = "City Name";
  address.sorting_code = "123CEDEX";

  std::vector<std::string> actual;
  address.FormatForDisplay(&actual);

  std::vector<std::string> expected(
      1,
      address.sorting_code + " " +
          address.address_lines[0] + " " +
          address.locality + " " +
          address.sorting_code);

  EXPECT_EQ(expected, actual);
}

TEST(AddressDataTest, FormatForDisplayWithoutStreetCi) {
  AddressData address;
  address.country_code = "CI";
  address.locality = "City Name";
  address.sorting_code = "123CEDEX";

  std::vector<std::string> actual;
  address.FormatForDisplay(&actual);

  std::vector<std::string> expected(
      1,
      address.sorting_code + "  " +
          address.locality + " " +
          address.sorting_code);

  EXPECT_EQ(expected, actual);
}

}  // namespace
