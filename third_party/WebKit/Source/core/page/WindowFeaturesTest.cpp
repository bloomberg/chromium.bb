// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "core/page/CreateWindow.h"

#include <gtest/gtest.h>
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebWindowFeatures.h"

namespace blink {

using WindowFeaturesTest = testing::Test;

TEST_F(WindowFeaturesTest, NoOpener) {
  static const struct {
    const char* feature_string;
    bool noopener;
  } kCases[] = {
      {"", false},
      {"something", false},
      {"something, something", false},
      {"notnoopener", false},
      {"noopener", true},
      {"something, noopener", true},
      {"noopener, something", true},
      {"NoOpEnEr", true},
  };

  for (const auto& test : kCases) {
    EXPECT_EQ(test.noopener,
              GetWindowFeaturesFromString(test.feature_string).noopener)
        << "Testing '" << test.feature_string << "'";
  }
}

}  // namespace blink
