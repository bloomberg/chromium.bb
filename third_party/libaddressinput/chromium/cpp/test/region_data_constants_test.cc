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

#include "region_data_constants.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

using i18n::addressinput::RegionDataConstants;

// Returns AssertionSuccess if |data| begins with '{' and ends with '}'.
testing::AssertionResult HasCurlyBraces(const std::string& data) {
  if (data.empty()) {
    return testing::AssertionFailure() << "data is empty";
  }
  if (data[0] != '{') {
    return testing::AssertionFailure() << data << " does not start with '{'";
  }
  if (data[data.length() - 1] != '}') {
    return testing::AssertionFailure() << data << " does not end with '}'";
  }
  return testing::AssertionSuccess();
}

// Verifies that the default region data begins with '{' and ends with '}'.
TEST(DefaultRegionDataTest, DefaultRegionHasCurlyBraces) {
  EXPECT_TRUE(HasCurlyBraces(RegionDataConstants::GetDefaultRegionData()));
}

TEST(RegionDataTest, RegionDataHasCertainProperties) {
  const std::vector<std::string>& region_data_codes =
      RegionDataConstants::GetRegionCodes();
  for (size_t i = 0; i < region_data_codes.size(); ++i) {
    SCOPED_TRACE("For region: " + region_data_codes[i]);
    EXPECT_EQ(2U, region_data_codes[i].length());

    const std::string& region_data = RegionDataConstants::GetRegionData(
        region_data_codes[i]);

    // Verifies that a region data value begins with '{' and end with '}', for
    // example "{\"fmt\":\"%C%S\"}".
    EXPECT_TRUE(HasCurlyBraces(region_data));

    // Verifies that a region data value contains a "name" key, for example
    // "{\"name\":\"SOUTH AFRICA\"}".
    EXPECT_NE(std::string::npos, region_data.find("\"name\":"));
  }
}

}  // namespace
