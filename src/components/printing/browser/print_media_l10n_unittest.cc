// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test is only built and run on platforms allowing print media
// localization.

#include <string>
#include <vector>

#include "components/printing/browser/print_media_l10n.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

// Verify that we localize some common names.
TEST(PrintMediaL10N, LocalizeSomeCommonNames) {
  const struct {
    const char* vendor_id;
    const char* expected_localized_name;
  } kTestCases[] = {
      {"na_c_17x22in", "Engineering-C"},
      {"iso_a0_841x1189mm", "A0"},
      {"iso_a1_594x841mm", "A1"},
      {"iso_a4_210x297mm", "A4"},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(LocalizePaperDisplayName(test_case.vendor_id),
              test_case.expected_localized_name);
  }
}

// Verify that we attempt to split and prettify a vendor ID for which
// we don't have a localization.
TEST(PrintMediaL10N, DoWithoutCommonName) {
  const struct {
    const char* vendor_id;
    const char* expected_localized_name;
  } kTestCases[] = {
      {"lorem_ipsum_8x10in", "lorem ipsum"},
      {"q_e_d_130x200mm", "q e d"},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(LocalizePaperDisplayName(test_case.vendor_id),
              test_case.expected_localized_name);
  }
}

// Verify that we return the vendor ID itself
//  1. when we don't have a localization and
//  2. when we don't see it split into at least 2 tokens (for name and
//     dimensions).
TEST(PrintMediaL10N, FallbackToVendorId) {
  const std::string no_dim = "I-BE-NAME-SANS-DIMENSIONS";
  EXPECT_EQ(LocalizePaperDisplayName(no_dim), no_dim);
}

}  // namespace printing
