// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/size_base.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"
#include "ui/gfx/size_f.h"

namespace ui {

static int test_sizef(const gfx::SizeF& s) {
  return s.width();
}

TEST(SizeTest, ToSizeF) {
  // Check that implicit conversion from integer to float compiles.
  gfx::Size a(10, 20);
  float width = test_sizef(a);
  EXPECT_EQ(width, a.width());

  gfx::SizeF b(10, 20);
  bool equals = a == b;
  EXPECT_EQ(true, equals);

  equals = b == a;
  EXPECT_EQ(true, equals);
}

}  // namespace ui
