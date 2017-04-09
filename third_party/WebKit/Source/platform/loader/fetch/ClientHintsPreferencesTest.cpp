// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/ClientHintsPreferences.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ClientHintsPreferencesTest : public ::testing::Test {};

TEST_F(ClientHintsPreferencesTest, Basic) {
  struct TestCase {
    const char* header_value;
    bool expectation_resource_width;
    bool expectation_dpr;
    bool expectation_viewport_width;
  } cases[] = {
      {"width, dpr, viewportWidth", true, true, false},
      {"WiDtH, dPr,     viewport-width", true, true, true},
      {"WIDTH, DPR, VIWEPROT-Width", true, true, false},
      {"VIewporT-Width, wutwut, width", true, false, true},
      {"dprw", false, false, false},
      {"DPRW", false, false, false},
  };

  for (const auto& test_case : cases) {
    ClientHintsPreferences preferences;
    const char* value = test_case.header_value;

    preferences.UpdateFromAcceptClientHintsHeader(value, nullptr);
    EXPECT_EQ(test_case.expectation_resource_width,
              preferences.ShouldSendResourceWidth());
    EXPECT_EQ(test_case.expectation_dpr, preferences.ShouldSendDPR());
    EXPECT_EQ(test_case.expectation_viewport_width,
              preferences.ShouldSendViewportWidth());
  }
}

}  // namespace blink
