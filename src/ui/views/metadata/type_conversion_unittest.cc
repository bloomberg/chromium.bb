// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/rect.h"

namespace MD = views::metadata;

using TypeConversionTest = PlatformTest;

TEST_F(TypeConversionTest, TestConversion_IntToString) {
  int from_int = 5;
  base::string16 to_string = MD::TypeConverter<int>::ToString(from_int);

  EXPECT_EQ(to_string, base::ASCIIToUTF16("5"));
}

TEST_F(TypeConversionTest, TestConversion_StringToInt) {
  base::string16 from_string = base::ASCIIToUTF16("10");
  EXPECT_EQ(MD::TypeConverter<int>::FromString(from_string), 10);
}

// This tests whether the converter handles a bogus input string, in which case
// the return value should be nullopt.
TEST_F(TypeConversionTest, TestConversion_BogusStringToInt) {
  base::string16 from_string = base::ASCIIToUTF16("Foo");
  EXPECT_EQ(MD::TypeConverter<int>::FromString(from_string), base::nullopt);
}

TEST_F(TypeConversionTest, TestConversion_BogusStringToFloat) {
  base::string16 from_string = base::ASCIIToUTF16("1.2");
  EXPECT_EQ(MD::TypeConverter<float>::FromString(from_string), 1.2f);
}

TEST_F(TypeConversionTest, TestConversion_OptionalIntToString) {
  base::Optional<int> src;
  base::string16 to_string =
      MD::TypeConverter<base::Optional<int>>::ToString(src);
  EXPECT_EQ(to_string, MD::GetNullOptStr());

  src = 5;
  to_string = MD::TypeConverter<base::Optional<int>>::ToString(src);
  EXPECT_EQ(to_string, base::ASCIIToUTF16("5"));
}

TEST_F(TypeConversionTest, TestConversion_StringToOptionalInt) {
  base::Optional<int> ret;
  EXPECT_EQ(
      MD::TypeConverter<base::Optional<int>>::FromString(MD::GetNullOptStr()),
      base::make_optional(ret));

  EXPECT_EQ(MD::TypeConverter<base::Optional<int>>::FromString(
                base::ASCIIToUTF16("10")),
            10);

  EXPECT_EQ(MD::TypeConverter<base::Optional<int>>::FromString(
                base::ASCIIToUTF16("ab0")),
            base::nullopt);
}
