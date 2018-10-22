// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/menu_label_accelerator_util.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(MenuLabelAcceleratorTest, GetMnemonic) {
  static const struct {
    const base::string16 label;
    const base::char16 mneumonic;
  } cases[] = {
      {base::ASCIIToUTF16(""), 0},         {base::ASCIIToUTF16("Exit"), 0},
      {base::ASCIIToUTF16("E&xit"), 'x'},  {base::ASCIIToUTF16("E&&xit"), 0},
      {base::ASCIIToUTF16("E&xi&t"), 'x'}, {base::ASCIIToUTF16("Exit&"), 0},
  };
  for (const auto& test : cases)
    EXPECT_EQ(GetMnemonic(test.label), test.mneumonic);
}

}  // namespace ui
