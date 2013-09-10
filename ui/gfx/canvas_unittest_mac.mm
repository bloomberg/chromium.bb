// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/utf_string_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"

namespace gfx {

namespace {

// Mac-specific code for string size computations. This is a verbatim copy
// of the old implementation that used to be in canvas_mac.mm.
void CanvasMac_SizeStringInt(const base::string16& text,
                             const FontList& font_list,
                             int* width,
                             int* height,
                             int line_height,
                             int flags) {
  DLOG_IF(WARNING, line_height != 0) << "Line heights not implemented.";
  DLOG_IF(WARNING, flags & Canvas::MULTI_LINE) << "Multi-line not implemented.";

  NSFont* native_font = font_list.GetPrimaryFont().GetNativeFont();
  NSString* ns_string = base::SysUTF16ToNSString(text);
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObject:native_font
                                  forKey:NSFontAttributeName];
  NSSize string_size = [ns_string sizeWithAttributes:attributes];
  *width = string_size.width;
  *height = font_list.GetHeight();
}

}  // namespace

class CanvasTestMac : public testing::Test {
 protected:
  // Compare the size returned by Canvas::SizeStringInt to the size generated
  // by the platform-specific version in CanvasMac_SizeStringInt. Will generate
  // expectation failure on any mismatch. Only works for single-line text
  // without specified line height, since that is all the platform
  // implementation supports.
  void CompareSizes(const char* text) {
    FontList font_list(font_);
    base::string16 text16 = base::UTF8ToUTF16(text);

    int mac_width = -1;
    int mac_height = -1;
    CanvasMac_SizeStringInt(text16, font_list, &mac_width, &mac_height, 0, 0);

    int canvas_width = -1;
    int canvas_height = -1;
    Canvas::SizeStringInt(
        text16, font_list, &canvas_width, &canvas_height, 0, 0);

    EXPECT_EQ(mac_width, canvas_width) << " width for " << text;
    EXPECT_EQ(mac_height, canvas_height) << " height for " << text;
  }

 private:
  Font font_;
};

 // Tests that Canvas' SizeStringInt yields result consistent with a native
 // implementation.
 TEST_F(CanvasTestMac, StringSizeIdenticalForSkia) {
  CompareSizes("");
  CompareSizes("Foo");
  CompareSizes("Longword");
  CompareSizes("This is a complete sentence.");
}

}  // namespace gfx
