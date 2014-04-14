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

#include <gtest/gtest.h>

#include "grit.h"

namespace i18n {
namespace addressinput {

namespace {

// Returns testing::AssertionSuccess if the |components| are valid.
testing::AssertionResult ComponentsAreValid(
    const std::vector<AddressUiComponent>& components) {
  if (components.empty()) {
    return testing::AssertionFailure() << "no components";
  }

  std::set<AddressField> fields;
  for (std::vector<AddressUiComponent>::const_iterator
           component_it = components.begin();
       component_it != components.end();
       ++component_it) {
    static const AddressField kMinAddressField = COUNTRY;
    static const AddressField kMaxAddressField = RECIPIENT;
    if (component_it->field < kMinAddressField ||
        component_it->field > kMaxAddressField) {
      return testing::AssertionFailure() << "unexpected input field "
                                         << component_it->field;
    }

    if (fields.find(component_it->field) != fields.end()) {
      return testing::AssertionFailure() << "duplicate input field "
                                         << component_it->field;
    }
    fields.insert(component_it->field);

    if (component_it->name_id == INVALID_MESSAGE_ID) {
      return testing::AssertionFailure() << "invalid field name_id for field "
                                         << component_it->field;
    }
  }

  return testing::AssertionSuccess();
}

// 1) Verifies that a region code consists of two characters, for example "TW".
// 2) Verifies that BuildComponents() returns valid UI components for a region
//    code.
// 3) Verifies that BuildComponents() returns a non-empty vector for a region
//    code.
TEST(AddressUiTest, RegionsAndComponentsAreValid) {
  const std::vector<std::string>& region_codes = GetRegionCodes();
  for (size_t i = 0; i < region_codes.size(); ++i) {
    SCOPED_TRACE("Region code: " + region_codes[i]);
    EXPECT_EQ(2U, region_codes[i].size());
    EXPECT_TRUE(ComponentsAreValid(
        BuildComponents(region_codes[i], std::string(), NULL)));
    EXPECT_FALSE(GetRequiredFields(region_codes[i]).empty());
  }
}

// Verifies that BuildComponents() and GetRequiredFields() return an empty
// vector for an invalid region code.
TEST(AddressUiTest, InvalidRegionCodeReturnsEmptyVector) {
  EXPECT_TRUE(
      BuildComponents("INVALID-REGION-CODE", std::string(), NULL).empty());
  EXPECT_TRUE(GetRequiredFields("INVALID-REGION-CODE").empty());
}

struct SeparatorData {
  SeparatorData(const std::string& language_code,
                const std::string& compact_line_separator)
      : language_code(language_code),
        compact_line_separator(compact_line_separator) {}

  ~SeparatorData() {}

  std::string language_code;
  std::string compact_line_separator;
};

// Tests for compact line separator.
class CompactLineSeparatorTest
  : public testing::TestWithParam<SeparatorData> {};

TEST_P(CompactLineSeparatorTest, BasicTest) {
  EXPECT_EQ(GetParam().compact_line_separator,
            GetCompactAddressLinesSeparator(GetParam().language_code));
}

INSTANTIATE_TEST_CASE_P(
    CompactLineSeparators, CompactLineSeparatorTest,
    testing::Values(
        SeparatorData("ja", ""),
        SeparatorData("zh", ""),
        SeparatorData("zh-hans", ""),
        SeparatorData("ar", "، "),
        SeparatorData("ko", " "),
        SeparatorData("th", " "),
        SeparatorData("en", ", ")));

TEST(AddressUiTest, ComponentLanguageCodeTest) {
  static const struct LanguageCodeData {
    const char* region_code;
    const char* ui_language_code;
    const char* components_language_code;
  } kLangugeCodes[] = {
    {"AM", "", "hy"},
    {"AM", "hy", "hy"},
    {"AM", "en", "hy-latn"},
    {"CN", "zh-hans", "zh-hans"},
    {"CN", "zh-hant", "zh-hant"},
    {"CN", "zh", "zh"},
    {"CN", "zh-latn", "zh-latn"},
    {"CN", "zh-Latn", "zh-latn"},
    {"CN", "zh-latn-US", "zh-latn"},
    {"CN", "zh-Latn-US", "zh-latn"},
    {"CN", "en", "zh-latn"},
    {"CN", "ja", "zh-latn"},
    {"CN", "ko", "zh-latn"},
    {"HK", "zh", "zh"},
    {"HK", "zh-hans", "zh-hans"},
    {"HK", "zh-hant", "zh-hant"},
    {"HK", "zh-latn", "zh-latn"},
    {"HK", "en", "en"},
    {"HK", "fr", "zh-latn"},
    {"HK", "ja", "zh-latn"},
    {"HK", "ko", "zh-latn"},
    {"MO", "zh", "zh"},
    {"MO", "pt", "pt"},
    {"MO", "en", "zh-latn"},
    {"AQ", "en", "en"},
    {"AQ", "fr", "fr"},
    {"AQ", "es", "es"},
    {"AQ", "zh", "zh"}
  };
  static const size_t kArraySize =
      sizeof kLangugeCodes / sizeof (LanguageCodeData);
  for (size_t i = 0; i < kArraySize; ++i) {
    SCOPED_TRACE(std::string("region code = ") +
        kLangugeCodes[i].region_code + ", ui language code = " +
        kLangugeCodes[i].ui_language_code + ", components language code = " +
        kLangugeCodes[i].components_language_code);
    std::string components_language_code;
    EXPECT_FALSE(BuildComponents(kLangugeCodes[i].region_code,
                                 kLangugeCodes[i].ui_language_code,
                                 &components_language_code).empty());
    EXPECT_EQ(
        kLangugeCodes[i].components_language_code, components_language_code);
  }
}

}  // namespace

}  // namespace addressinput
}  // namespace i18n
