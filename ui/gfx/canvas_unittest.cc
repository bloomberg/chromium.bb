// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"

namespace gfx {

TEST(CanvasTest, StringWidth) {
  const string16 text = UTF8ToUTF16("Test");
  const int width = Canvas::GetStringWidth(text, Font());

  EXPECT_GT(width, 0);
}

TEST(CanvasTest, StringWidthEmptyString) {
  const string16 text = UTF8ToUTF16("");
  const int width = Canvas::GetStringWidth(text, Font());

  EXPECT_EQ(0, width);
}

TEST(CanvasTest, StringSizeEmptyString) {
  const Font font;
  const string16 text = UTF8ToUTF16("");
  int width = 0;
  int height = 0;
  Canvas::SizeStringInt(text, font, &width, &height, 0);

  EXPECT_EQ(0, width);
  EXPECT_GT(height, 0);
}

}  // namespace gfx
