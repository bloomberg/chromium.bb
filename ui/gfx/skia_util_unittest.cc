// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_util.h"

#include "testing/gtest/include/gtest/gtest.h"

static const char kAcceleratorChar = '&';

TEST(SkiaUtilTest, RemoveAcceleratorChar) {
  EXPECT_TRUE(gfx::RemoveAcceleratorChar("", kAcceleratorChar).empty());
  EXPECT_TRUE(gfx::RemoveAcceleratorChar("&", kAcceleratorChar).empty());
  EXPECT_EQ(std::string("no accelerator"),
            gfx::RemoveAcceleratorChar("no accelerator", kAcceleratorChar));
  EXPECT_EQ(std::string("one accelerator"),
            gfx::RemoveAcceleratorChar("&one accelerator", kAcceleratorChar));
  EXPECT_EQ(std::string("one accelerator"),
            gfx::RemoveAcceleratorChar("one &accelerator", kAcceleratorChar));
  EXPECT_EQ(std::string("one_accelerator"),
            gfx::RemoveAcceleratorChar("one_accelerator&", kAcceleratorChar));
  EXPECT_EQ(std::string("two accelerators"),
            gfx::RemoveAcceleratorChar("&two &accelerators", kAcceleratorChar));
  EXPECT_EQ(std::string("two accelerators"),
            gfx::RemoveAcceleratorChar("two &accelerators&", kAcceleratorChar));
  EXPECT_EQ(std::string("two accelerators"),
            gfx::RemoveAcceleratorChar("two& &accelerators", kAcceleratorChar));
  EXPECT_EQ(std::string("&escaping"),
            gfx::RemoveAcceleratorChar("&&escaping", kAcceleratorChar));
  EXPECT_EQ(std::string("escap&ing"),
            gfx::RemoveAcceleratorChar("escap&&ing", kAcceleratorChar));
  EXPECT_EQ(std::string("escaping&"),
            gfx::RemoveAcceleratorChar("escaping&&", kAcceleratorChar));
  EXPECT_EQ(std::string("mix&ed"),
            gfx::RemoveAcceleratorChar("&mix&&ed", kAcceleratorChar));
  EXPECT_EQ(std::string("&mix&ed"),
            gfx::RemoveAcceleratorChar("&&m&ix&&e&d&", kAcceleratorChar));
  EXPECT_EQ(std::string("&m&ixed&"),
            gfx::RemoveAcceleratorChar("&&m&&ix&ed&&", kAcceleratorChar));
  EXPECT_EQ(std::string("m&ixed&"),
            gfx::RemoveAcceleratorChar("&m&&ix&ed&&", kAcceleratorChar));
}
