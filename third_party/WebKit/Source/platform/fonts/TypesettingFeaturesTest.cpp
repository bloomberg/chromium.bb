// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/TypesettingFeatures.h"

#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TypesettingFeaturesTest, ToString) {
  {
    TypesettingFeatures features = 0;
    EXPECT_EQ("", ToString(features));
  }
  {
    TypesettingFeatures features = kKerning | kLigatures;
    EXPECT_EQ("Kerning,Ligatures", ToString(features));
  }
  {
    TypesettingFeatures features = kKerning | kLigatures | kCaps;
    EXPECT_EQ("Kerning,Ligatures,Caps", ToString(features));
  }
}

}  // namespace blink
