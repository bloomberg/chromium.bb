// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/test/fontconfig_util_linux.h"

namespace gfx {

namespace {

// Loads the first system font defined by fontconfig_util_linux.h with a base
// filename of |basename|. Case is ignored.
bool LoadSystemFont(const std::string& basename) {
  for (size_t i = 0; i < kNumSystemFontsForFontconfig; ++i) {
    base::FilePath path(gfx::kSystemFontsForFontconfig[i]);
    if (strcasecmp(path.BaseName().value().c_str(), basename.c_str()) == 0)
      return LoadFontIntoFontconfig(path);
  }
  LOG(ERROR) << "Unable to find system font named " << basename;
  return false;
}

}  // namespace

class FontRenderParamsTest : public testing::Test {
 public:
  FontRenderParamsTest() {
    SetUpFontconfig();
    CHECK(temp_dir_.CreateUniqueTempDir());
  }

  virtual ~FontRenderParamsTest() {
    TearDownFontconfig();
  }

 protected:
  base::ScopedTempDir temp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FontRenderParamsTest);
};

TEST_F(FontRenderParamsTest, Default) {
  // Fontconfig needs to know about at least one font to return a match.
  ASSERT_TRUE(LoadSystemFont("arial.ttf"));
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(temp_dir_.path(),
      std::string(kFontconfigFileHeader) +
      kFontconfigMatchHeader +
      CreateFontconfigEditStanza("antialias", "bool", "true") +
      CreateFontconfigEditStanza("autohint", "bool", "false") +
      CreateFontconfigEditStanza("hinting", "bool", "true") +
      CreateFontconfigEditStanza("hintstyle", "const", "hintfull") +
      CreateFontconfigEditStanza("rgba", "const", "rgb") +
      kFontconfigMatchFooter +
      kFontconfigFileFooter));

  FontRenderParams params = GetDefaultFontRenderParams();
  EXPECT_TRUE(params.antialiasing);
  EXPECT_FALSE(params.autohinter);
  EXPECT_TRUE(params.use_bitmaps);
  EXPECT_EQ(FontRenderParams::HINTING_FULL, params.hinting);
  EXPECT_FALSE(params.subpixel_positioning);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_RGB,
            params.subpixel_rendering);
}

TEST_F(FontRenderParamsTest, Size) {
  // Fontconfig needs to know about at least one font to return a match.
  ASSERT_TRUE(LoadSystemFont("arial.ttf"));
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(temp_dir_.path(),
      std::string(kFontconfigFileHeader) +
      kFontconfigMatchHeader +
      CreateFontconfigEditStanza("antialias", "bool", "true") +
      CreateFontconfigEditStanza("hinting", "bool", "true") +
      CreateFontconfigEditStanza("hintstyle", "const", "hintfull") +
      CreateFontconfigEditStanza("rgba", "const", "none") +
      kFontconfigMatchFooter +
      kFontconfigMatchHeader +
      CreateFontconfigTestStanza("pixelsize", "less_eq", "double", "10") +
      CreateFontconfigEditStanza("antialias", "bool", "false") +
      kFontconfigMatchFooter +
      kFontconfigMatchHeader +
      CreateFontconfigTestStanza("size", "more_eq", "double", "20") +
      CreateFontconfigEditStanza("hintstyle", "const", "hintslight") +
      CreateFontconfigEditStanza("rgba", "const", "rgb") +
      kFontconfigMatchFooter +
      kFontconfigFileFooter));

  // The defaults should be used when the supplied size isn't matched by the
  // second or third blocks.
  int pixel_size = 12;
  FontRenderParams params = GetCustomFontRenderParams(
      false, NULL, &pixel_size, NULL, NULL);
  EXPECT_TRUE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_FULL, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_NONE,
            params.subpixel_rendering);

  pixel_size = 10;
  params = GetCustomFontRenderParams(false, NULL, &pixel_size, NULL, NULL);
  EXPECT_FALSE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_FULL, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_NONE,
            params.subpixel_rendering);

  int point_size = 20;
  params = GetCustomFontRenderParams(false, NULL, NULL, &point_size, NULL);
  EXPECT_TRUE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_SLIGHT, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_RGB,
            params.subpixel_rendering);
}

}  // namespace gfx
