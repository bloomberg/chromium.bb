// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/fontconfig_util.h"

#include <fontconfig/fontconfig.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(FontConfigUtilTest, GetFontRenderParamsFromFcPatternWithEmptyPattern) {
  FcPattern* pattern = FcPatternCreate();

  FontRenderParams params;
  GetFontRenderParamsFromFcPattern(pattern, &params);
  FcPatternDestroy(pattern);

  FontRenderParams empty_params;
  EXPECT_EQ(params, empty_params);
}

TEST(FontConfigUtilTest, GetFontRenderParamsFromFcPatternWithFalseValues) {
  FcPattern* pattern = FcPatternCreate();
  FcPatternAddBool(pattern, FC_ANTIALIAS, FcFalse);
  FcPatternAddBool(pattern, FC_AUTOHINT, FcFalse);
  FcPatternAddBool(pattern, FC_EMBEDDED_BITMAP, FcFalse);

  FontRenderParams params;
  GetFontRenderParamsFromFcPattern(pattern, &params);
  FcPatternDestroy(pattern);

  FontRenderParams expected_params;
  expected_params.antialiasing = false;
  expected_params.autohinter = false;
  expected_params.use_bitmaps = false;
  EXPECT_EQ(params, expected_params);
}

TEST(FontConfigUtilTest, GetFontRenderParamsFromFcPatternWithValues) {
  FcPattern* pattern = FcPatternCreate();
  FcPatternAddBool(pattern, FC_ANTIALIAS, FcTrue);
  FcPatternAddBool(pattern, FC_AUTOHINT, FcTrue);
  FcPatternAddInteger(pattern, FC_HINT_STYLE, FC_HINT_MEDIUM);
  FcPatternAddBool(pattern, FC_EMBEDDED_BITMAP, FcTrue);
  FcPatternAddInteger(pattern, FC_RGBA, FC_RGBA_RGB);

  FontRenderParams params;
  GetFontRenderParamsFromFcPattern(pattern, &params);
  FcPatternDestroy(pattern);

  FontRenderParams expected_params;
  expected_params.antialiasing = true;
  expected_params.autohinter = true;
  expected_params.hinting = FontRenderParams::HINTING_MEDIUM;
  expected_params.use_bitmaps = true;
  expected_params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;

  EXPECT_EQ(params, expected_params);
}

}  // namespace gfx
