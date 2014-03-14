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
    EXPECT_TRUE(ComponentsAreValid(BuildComponents(region_codes[i])));
    EXPECT_FALSE(GetRequiredFields(region_codes[i]).empty());
  }
}

// Verifies that BuildComponents() and GetRequiredFields() return an empty
// vector for an invalid region code.
TEST(AddressUiTest, InvalidRegionCodeReturnsEmptyVector) {
  EXPECT_TRUE(BuildComponents("INVALID-REGION-CODE").empty());
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


}  // namespace

}  // namespace addressinput
}  // namespace i18n
