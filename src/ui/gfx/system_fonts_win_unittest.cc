// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/system_fonts_win.h"

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {
namespace win {

namespace {

LOGFONT CreateLOGFONT(const base::char16* name, LONG height) {
  LOGFONT logfont = {};
  logfont.lfHeight = height;
  auto result = wcscpy_s(logfont.lfFaceName, name);
  DCHECK_EQ(0, result);
  return logfont;
}

const base::char16 kSegoeUI[] = L"Segoe UI";
const base::char16 kArial[] = L"Arial";

}  // namespace

TEST(SystemFontsWinTest, AdjustFontSize) {
  gfx::win::SetGetMinimumFontSizeCallback(nullptr);
  EXPECT_EQ(10, gfx::win::AdjustFontSize(10, 0));
  EXPECT_EQ(-10, gfx::win::AdjustFontSize(-10, 0));
  EXPECT_EQ(8, gfx::win::AdjustFontSize(10, -2));
  EXPECT_EQ(-8, gfx::win::AdjustFontSize(-10, -2));
  EXPECT_EQ(13, gfx::win::AdjustFontSize(10, 3));
  EXPECT_EQ(-13, gfx::win::AdjustFontSize(-10, 3));
  EXPECT_EQ(1, gfx::win::AdjustFontSize(10, -9));
  EXPECT_EQ(-1, gfx::win::AdjustFontSize(-10, -9));
  EXPECT_EQ(0, gfx::win::AdjustFontSize(10, -12));
  EXPECT_EQ(0, gfx::win::AdjustFontSize(-10, -12));
}

TEST(SystemFontsWinTest, AdjustFontSize_MinimumSizeSpecified) {
  gfx::win::SetGetMinimumFontSizeCallback([] { return 1; });
  EXPECT_EQ(10, gfx::win::AdjustFontSize(10, 0));
  EXPECT_EQ(-10, gfx::win::AdjustFontSize(-10, 0));
  EXPECT_EQ(8, gfx::win::AdjustFontSize(10, -2));
  EXPECT_EQ(-8, gfx::win::AdjustFontSize(-10, -2));
  EXPECT_EQ(13, gfx::win::AdjustFontSize(10, 3));
  EXPECT_EQ(-13, gfx::win::AdjustFontSize(-10, 3));
  EXPECT_EQ(1, gfx::win::AdjustFontSize(10, -9));
  EXPECT_EQ(-1, gfx::win::AdjustFontSize(-10, -9));
  EXPECT_EQ(1, gfx::win::AdjustFontSize(10, -12));
  EXPECT_EQ(-1, gfx::win::AdjustFontSize(-10, -12));
}

TEST(SystemFontsWinTest, AdjustLOGFONT_NoAdjustment) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  FontAdjustment adjustment;
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-12, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);
}

TEST(SystemFontsWinTest, AdjustLOGFONT_ChangeFace) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  FontAdjustment adjustment{kArial, 1.0};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-12, logfont.lfHeight);
  EXPECT_STREQ(kArial, logfont.lfFaceName);
}

TEST(SystemFontsWinTest, AdjustLOGFONT_ScaleDown) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  FontAdjustment adjustment{L"", 0.5};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-6, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 12);
  adjustment = {L"", 0.5};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(6, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);
}

TEST(SystemFontsWinTest, AdjustLOGFONT_ScaleDownWithRounding) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -10);
  FontAdjustment adjustment{L"", 0.85};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-9, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 10);
  adjustment = {L"", 0.85};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(9, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);
}

TEST(SystemFontsWinTest, AdjustLOGFONT_ScaleUpWithFaceChange) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -12);
  FontAdjustment adjustment{kArial, 1.5};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-18, logfont.lfHeight);
  EXPECT_STREQ(kArial, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 12);
  adjustment = {kArial, 1.5};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(18, logfont.lfHeight);
  EXPECT_STREQ(kArial, logfont.lfFaceName);
}

TEST(SystemFontsWinTest, AdjustLOGFONT_ScaleUpWithRounding) {
  LOGFONT logfont = CreateLOGFONT(kSegoeUI, -10);
  FontAdjustment adjustment{L"", 1.111};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(-11, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);

  logfont = CreateLOGFONT(kSegoeUI, 10);
  adjustment = {L"", 1.11};
  AdjustLOGFONTForTesting(adjustment, &logfont);
  EXPECT_EQ(11, logfont.lfHeight);
  EXPECT_STREQ(kSegoeUI, logfont.lfFaceName);
}

}  // namespace win
}  // namespace gfx
