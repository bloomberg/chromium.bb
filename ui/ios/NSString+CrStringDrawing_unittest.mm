// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ui/ios/NSString+CrStringDrawing.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

typedef PlatformTest NSStringCrStringDrawing;

// These test verifies that the category methods return the same values as the
// deprecated methods, so ignore warnings about using deprecated methods.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

TEST_F(NSStringCrStringDrawing, SizeWithFont) {
  NSArray* fonts = @[
    [NSNull null],
    [UIFont systemFontOfSize:16],
    [UIFont boldSystemFontOfSize:10],
    [UIFont fontWithName:@"Helvetica" size:12.0],
  ];
  for (UIFont* font in fonts) {
    if ([font isEqual:[NSNull null]])
      font = nil;
    std::string font_tag = "with font ";
    font_tag.append(
        base::SysNSStringToUTF8(font ? [font description] : @"nil"));
    EXPECT_EQ([@"" sizeWithFont:font].width,
              [@"" cr_sizeWithFont:font].width) << font_tag;
    EXPECT_EQ([@"" sizeWithFont:font].height,
              [@"" cr_sizeWithFont:font].height) << font_tag;
    EXPECT_EQ([@"Test" sizeWithFont:font].width,
              [@"Test" cr_sizeWithFont:font].width) << font_tag;
    EXPECT_EQ([@"Test" sizeWithFont:font].height,
              [@"Test" cr_sizeWithFont:font].height) << font_tag;
    EXPECT_EQ([@"你好" sizeWithFont:font].width,
              [@"你好" cr_sizeWithFont:font].width) << font_tag;
    EXPECT_EQ([@"你好" sizeWithFont:font].height,
              [@"你好" cr_sizeWithFont:font].height) << font_tag;
    NSString* long_string = @"★ This is a test string that is very long.";
    EXPECT_EQ([long_string sizeWithFont:font].width,
              [long_string cr_sizeWithFont:font].width) << font_tag;
    EXPECT_EQ([long_string sizeWithFont:font].height,
              [long_string cr_sizeWithFont:font].height) << font_tag;
  }
}
#pragma clang diagnostic pop  // ignored "-Wdeprecated-declarations"

TEST_F(NSStringCrStringDrawing, PixelAlignedSizeWithFont) {
  NSArray* fonts = @[
    [UIFont systemFontOfSize:16],
    [UIFont boldSystemFontOfSize:10],
    [UIFont fontWithName:@"Helvetica" size:12.0],
  ];
  NSArray* strings = @[
    @"",
    @"Test",
    @"你好",
    @"★ This is a test string that is very long.",
  ];
  for (UIFont* font in fonts) {
    NSDictionary* attributes = @{ NSFontAttributeName : font };

    for (NSString* string in strings) {
      std::string test_tag = base::StringPrintf("for string '%s' with font %s",
          base::SysNSStringToUTF8(string).c_str(),
          base::SysNSStringToUTF8([font description]).c_str());

      CGSize size_with_attributes = [string sizeWithAttributes:attributes];
      CGSize size_with_pixel_aligned =
          [string cr_pixelAlignedSizeWithFont:font];

      // Verify that the pixel_aligned size is always rounded up (i.e. the size
      // returned from sizeWithAttributes: is less than or equal to the pixel-
      // aligned size).
      EXPECT_LE(size_with_attributes.width,
                size_with_pixel_aligned.width) << test_tag;
      EXPECT_LE(size_with_attributes.height,
                size_with_pixel_aligned.height) << test_tag;

      // Verify that the pixel_aligned size is never more than a pixel different
      // than the size returned from sizeWithAttributes:.
      static CGFloat scale = [[UIScreen mainScreen] scale];
      EXPECT_NEAR(size_with_attributes.width * scale,
                  size_with_pixel_aligned.width * scale,
                  0.9999) << test_tag;
      EXPECT_NEAR(size_with_attributes.height * scale,
                  size_with_pixel_aligned.height * scale,
                  0.9999) << test_tag;

      // Verify that the pixel-aligned value is pixel-aligned.
      EXPECT_FLOAT_EQ(roundf(size_with_pixel_aligned.width * scale),
                      size_with_pixel_aligned.width * scale) << test_tag;
      EXPECT_FLOAT_EQ(roundf(size_with_pixel_aligned.height * scale),
                      size_with_pixel_aligned.height * scale) << test_tag;
    }
  }
}


}  // namespace
