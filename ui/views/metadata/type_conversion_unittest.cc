// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/rect.h"

namespace TC = views::metadata;

using TypeConversionTest = PlatformTest;

TEST_F(TypeConversionTest, TestConversion_IntToString) {
  int from_int = 5;
  base::string16 to_string = TC::Convert<int, base::string16>(from_int);

  EXPECT_EQ(to_string, base::ASCIIToUTF16("5"));
}

TEST_F(TypeConversionTest, TestConversion_StringToInt) {
  base::string16 from_string = base::ASCIIToUTF16("10");
  int to_int = TC::Convert<base::string16, int>(from_string);

  EXPECT_EQ(to_int, 10);
}
