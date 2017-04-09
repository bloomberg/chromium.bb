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

void TestSystemFontContainsString(FontWeight desired_weight,
                                  NSString* substring) {
  NSFont* font =
      MatchNSFontFamily(FontFamilyNames::system_ui, 0, desired_weight, 11);
  EXPECT_TRUE([font.description containsString:substring]);
}

TEST(FontFamilyMatcherMacTest, YosemiteFontWeights) {
  if (!IsOS10_10())
    return;

  TestSystemFontContainsString(kFontWeight100, @"-UltraLight");
  TestSystemFontContainsString(kFontWeight200, @"-Thin");
  TestSystemFontContainsString(kFontWeight300, @"-Light");
  TestSystemFontContainsString(kFontWeight400, @"-Regular");
  TestSystemFontContainsString(kFontWeight500, @"-Medium");
  TestSystemFontContainsString(kFontWeight600, @"-Bold");
  TestSystemFontContainsString(kFontWeight700, @"-Bold");
  TestSystemFontContainsString(kFontWeight800, @"-Heavy");
  TestSystemFontContainsString(kFontWeight900, @"-Heavy");
}

}  // namespace blink
