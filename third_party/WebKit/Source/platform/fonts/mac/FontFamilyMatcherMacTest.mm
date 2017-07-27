// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "platform/fonts/mac/FontFamilyMatcherMac.h"

#include "platform/FontFamilyNames.h"

#include <AppKit/AppKit.h>
#include <gtest/gtest.h>

#import "platform/mac/VersionUtilMac.h"

@interface NSString (YosemiteAdditions)
- (BOOL)containsString:(NSString*)string;
@end

namespace blink {

void TestSystemFontContainsString(FontSelectionValue desired_weight,
                                  NSString* substring) {
  NSFont* font =
      MatchNSFontFamily(FontFamilyNames::system_ui, 0, desired_weight, 11);
  EXPECT_TRUE([font.description containsString:substring]);
}

TEST(FontFamilyMatcherMacTest, YosemiteFontWeights) {
  if (!IsOS10_10())
    return;

  TestSystemFontContainsString(FontSelectionValue(100), @"-UltraLight");
  TestSystemFontContainsString(FontSelectionValue(200), @"-Thin");
  TestSystemFontContainsString(FontSelectionValue(300), @"-Light");
  TestSystemFontContainsString(FontSelectionValue(400), @"-Regular");
  TestSystemFontContainsString(FontSelectionValue(500), @"-Medium");
  TestSystemFontContainsString(FontSelectionValue(600), @"-Bold");
  TestSystemFontContainsString(FontSelectionValue(700), @"-Bold");
  TestSystemFontContainsString(FontSelectionValue(800), @"-Heavy");
  TestSystemFontContainsString(FontSelectionValue(900), @"-Heavy");
}

}  // namespace blink
