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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "grit.h"

namespace i18n {
namespace addressinput {

namespace {

// Returns testing::AssertionSuccess if the |components| are valid. Uses
// |region_code| in test failure messages.
testing::AssertionResult ComponentsAreValid(
    const std::vector<AddressUiComponent>& components) {
  if (components.empty()) {
    return testing::AssertionFailure() << "no components";
  }

  for (std::vector<AddressUiComponent>::const_iterator
       component_it = components.begin();
       component_it != components.end(); ++component_it) {
    static const AddressField kMinAddressField = COUNTRY;
    static const AddressField kMaxAddressField = RECIPIENT;
    if (component_it->field < kMinAddressField ||
        component_it->field > kMaxAddressField) {
      return testing::AssertionFailure() << "unexpected field "
                                         << component_it->field;
    }

    if (component_it->name_id == INVALID_MESSAGE_ID) {
      return testing::AssertionFailure() << "invalid field name_id for field "
                                         << component_it->field;
    }
  }

  return testing::AssertionSuccess();
}

// Tests for address UI functions.
class AddressUiTest : public testing::TestWithParam<std::string> {};

// Verifies that a region code consists of two characters, for example "TW".
TEST_P(AddressUiTest, RegionCodeHasTwoCharacters) {
  EXPECT_EQ(2, GetParam().size());
}

// Verifies that BuildComponents() returns valid UI components for a region
// code.
TEST_P(AddressUiTest, ComponentsAreValid) {
  EXPECT_TRUE(ComponentsAreValid(BuildComponents(GetParam())));
}

// Test all regions codes.
INSTANTIATE_TEST_CASE_P(
    AllRegions, AddressUiTest,
    testing::ValuesIn(GetRegionCodes()));

// Verifies that BuildComponents() returns an empty vector for an invalid region
// code.
TEST_F(AddressUiTest, InvalidRegionCodeReturnsEmptyVector) {
  EXPECT_TRUE(BuildComponents("INVALID-REGION-CODE").empty());
}

}  // namespace

}  // namespace addressinput
}  // namespace i18n
