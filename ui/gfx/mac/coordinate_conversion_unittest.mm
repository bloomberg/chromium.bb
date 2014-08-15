// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/gfx/mac/coordinate_conversion.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#import "testing/platform_test.h"
#include "ui/gfx/geometry/rect.h"

const int kTestWidth = 320;
const int kTestHeight = 200;

// Class to donate an implementation of -[NSScreen frame] that provides a known
// value for robust tests.
@interface MacCoordinateConversionTestScreenDonor : NSObject
- (NSRect)frame;
@end

@implementation MacCoordinateConversionTestScreenDonor
- (NSRect)frame {
  return NSMakeRect(0, 0, kTestWidth, kTestHeight);
}
@end

namespace gfx {
namespace {

class MacCoordinateConversionTest : public PlatformTest {
 public:
  MacCoordinateConversionTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 private:
  scoped_ptr<base::mac::ScopedObjCClassSwizzler> swizzle_frame_;

  DISALLOW_COPY_AND_ASSIGN(MacCoordinateConversionTest);
};

void MacCoordinateConversionTest::SetUp() {
  // Before swizzling, do a sanity check that the primary screen's origin is
  // (0, 0). This should always be true.
  NSRect primary_screen_frame = [[[NSScreen screens] objectAtIndex:0] frame];
  EXPECT_EQ(0, primary_screen_frame.origin.x);
  EXPECT_EQ(0, primary_screen_frame.origin.y);

  swizzle_frame_.reset(new base::mac::ScopedObjCClassSwizzler(
      [NSScreen class],
      [MacCoordinateConversionTestScreenDonor class],
      @selector(frame)));

  primary_screen_frame = [[[NSScreen screens] objectAtIndex:0] frame];
  EXPECT_EQ(kTestWidth, primary_screen_frame.size.width);
  EXPECT_EQ(kTestHeight, primary_screen_frame.size.height);
}

void MacCoordinateConversionTest::TearDown() {
  swizzle_frame_.reset();
}

}  // namespace

// Tests for coordinate conversion on Mac. Start with the following setup:
// AppKit ....... gfx
// 199              0
// 189             10   Window of height 40 fills in pixel
// 179  ---------  20   at index 20
// 169  |       |  30   through
// ...  :       :  ..   to
// 150  |       |  49   pixel
// 140  ---------  59   at index 59
// 130             69   (inclusive).
//  ..             ..
//   0            199
TEST_F(MacCoordinateConversionTest, ScreenRectToFromNSRect) {
  Rect gfx_rect = Rect(10, 20, 30, 40);
  NSRect ns_rect = ScreenRectToNSRect(gfx_rect);
  EXPECT_TRUE(NSEqualRects(NSMakeRect(10, 140, 30, 40), ns_rect));
  EXPECT_EQ(gfx_rect.ToString(), ScreenRectFromNSRect(ns_rect).ToString());

  // Window in a screen to the left of the primary screen.
  gfx_rect = Rect(-40, 20, 30, 40);
  ns_rect = ScreenRectToNSRect(gfx_rect);
  EXPECT_TRUE(NSEqualRects(NSMakeRect(-40, 140, 30, 40), ns_rect));
  EXPECT_EQ(gfx_rect.ToString(), ScreenRectFromNSRect(ns_rect).ToString());

  // Window in a screen below the primary screen.
  gfx_rect = Rect(10, 220, 30, 40);
  ns_rect = ScreenRectToNSRect(gfx_rect);
  EXPECT_TRUE(NSEqualRects(NSMakeRect(10, -60, 30, 40), ns_rect));
  EXPECT_EQ(gfx_rect.ToString(), ScreenRectFromNSRect(ns_rect).ToString());

  // Window in a screen below and to the left primary screen.
  gfx_rect = Rect(-40, 220, 30, 40);
  ns_rect = ScreenRectToNSRect(gfx_rect);
  EXPECT_TRUE(NSEqualRects(NSMakeRect(-40, -60, 30, 40), ns_rect));
  EXPECT_EQ(gfx_rect.ToString(), ScreenRectFromNSRect(ns_rect).ToString());
}

}  // namespace gfx
