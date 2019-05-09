// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/rect.h"

namespace VM = views::metadata;

using TypeConversionTest = PlatformTest;

TEST_F(TypeConversionTest, TestConversion_IntToString) {
  int from_int = 5;
  base::string16 to_string = VM::Convert<int, base::string16>(from_int);

  EXPECT_EQ(to_string, base::ASCIIToUTF16("5"));
}

TEST_F(TypeConversionTest, TestConversion_StringToInt) {
  base::string16 from_string = base::ASCIIToUTF16("10");
  int to_int = VM::Convert<base::string16, int>(from_string, 0);

  EXPECT_EQ(to_int, 10);
}

// This tests whether the converter handles a bogus input string, in which case
// the default value should be returned.
TEST_F(TypeConversionTest, TestConversion_StringToIntDefault) {
  const int default_value = 1000;
  base::string16 from_string = base::ASCIIToUTF16("Foo");
  int to_int = VM::Convert<base::string16, int>(from_string, default_value);

  EXPECT_EQ(to_int, default_value);
}
