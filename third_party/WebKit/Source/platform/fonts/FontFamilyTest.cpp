// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontFamily.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

FontFamily* CreateAndAppendFamily(FontFamily& parent, const char* name) {
  RefPtr<SharedFontFamily> family = SharedFontFamily::Create();
  family->SetFamily(name);
  parent.AppendFamily(family);
  return family.get();
}

}  // namespace

TEST(FontFamilyTest, ToString) {
  {
    FontFamily family;
    EXPECT_EQ("", family.ToString());
  }
  {
    FontFamily family;
    family.SetFamily("A");
    CreateAndAppendFamily(family, "B");
    EXPECT_EQ("A,B", family.ToString());
  }
  {
    FontFamily family;
    family.SetFamily("A");
    FontFamily* b_family = CreateAndAppendFamily(family, "B");
    CreateAndAppendFamily(*b_family, "C");
    EXPECT_EQ("A,B,C", family.ToString());
  }
}

}  // namespace blink
