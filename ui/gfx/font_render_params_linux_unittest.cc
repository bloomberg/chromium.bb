// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/linux_font_delegate.h"
#include "ui/gfx/pango_util.h"
#include "ui/gfx/test/fontconfig_util_linux.h"

namespace gfx {

namespace {

// Implementation of LinuxFontDelegate that returns a canned FontRenderParams
// struct. This is used to isolate tests from the system's local configuration.
class TestFontDelegate : public LinuxFontDelegate {
 public:
  TestFontDelegate() {}
  virtual ~TestFontDelegate() {}

  void set_params(const FontRenderParams& params) { params_ = params; }

  virtual FontRenderParams GetDefaultFontRenderParams() const OVERRIDE {
    return params_;
  }
  virtual scoped_ptr<ScopedPangoFontDescription>
      GetDefaultPangoFontDescription() const OVERRIDE {
    NOTIMPLEMENTED();
    return scoped_ptr<ScopedPangoFontDescription>();
  }
  virtual double GetFontDPI() const OVERRIDE {
    NOTIMPLEMENTED();
    return 96.0;
  }

 private:
  FontRenderParams params_;

  DISALLOW_COPY_AND_ASSIGN(TestFontDelegate);
};

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
    original_font_delegate_ = LinuxFontDelegate::instance();
    LinuxFontDelegate::SetInstance(&test_font_delegate_);
  }

  virtual ~FontRenderParamsTest() {
    LinuxFontDelegate::SetInstance(
        const_cast<LinuxFontDelegate*>(original_font_delegate_));
    TearDownFontconfig();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  const LinuxFontDelegate* original_font_delegate_;
  TestFontDelegate test_font_delegate_;

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
      false, NULL, &pixel_size, NULL, NULL, NULL);
  EXPECT_TRUE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_FULL, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_NONE,
            params.subpixel_rendering);

  pixel_size = 10;
  params = GetCustomFontRenderParams(
      false, NULL, &pixel_size, NULL, NULL, NULL);
  EXPECT_FALSE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_FULL, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_NONE,
            params.subpixel_rendering);

  int point_size = 20;
  params = GetCustomFontRenderParams(
      false, NULL, NULL, &point_size, NULL, NULL);
  EXPECT_TRUE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_SLIGHT, params.hinting);
  EXPECT_EQ(FontRenderParams::SUBPIXEL_RENDERING_RGB,
            params.subpixel_rendering);
}

TEST_F(FontRenderParamsTest, Style) {
  ASSERT_TRUE(LoadSystemFont("arial.ttf"));
  // Load a config that disables antialiasing for bold text and disables
  // hinting for italic text.
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(temp_dir_.path(),
      std::string(kFontconfigFileHeader) +
      kFontconfigMatchHeader +
      CreateFontconfigEditStanza("antialias", "bool", "true") +
      CreateFontconfigEditStanza("hinting", "bool", "true") +
      CreateFontconfigEditStanza("hintstyle", "const", "hintfull") +
      kFontconfigMatchFooter +
      kFontconfigMatchHeader +
      CreateFontconfigTestStanza("weight", "eq", "const", "bold") +
      CreateFontconfigEditStanza("antialias", "bool", "false") +
      kFontconfigMatchFooter +
      kFontconfigMatchHeader +
      CreateFontconfigTestStanza("slant", "eq", "const", "italic") +
      CreateFontconfigEditStanza("hinting", "bool", "false") +
      kFontconfigMatchFooter +
      kFontconfigFileFooter));

  int style = Font::NORMAL;
  FontRenderParams params = GetCustomFontRenderParams(
      false, NULL, NULL, NULL, &style, NULL);
  EXPECT_TRUE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_FULL, params.hinting);

  style = Font::BOLD;
  params = GetCustomFontRenderParams(
      false, NULL, NULL, NULL, &style, NULL);
  EXPECT_FALSE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_FULL, params.hinting);

  style = Font::ITALIC;
  params = GetCustomFontRenderParams(
      false, NULL, NULL, NULL, &style, NULL);
  EXPECT_TRUE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_NONE, params.hinting);

  style = Font::BOLD | Font::ITALIC;
  params = GetCustomFontRenderParams(
      false, NULL, NULL, NULL, &style, NULL);
  EXPECT_FALSE(params.antialiasing);
  EXPECT_EQ(FontRenderParams::HINTING_NONE, params.hinting);
}

TEST_F(FontRenderParamsTest, Scalable) {
  ASSERT_TRUE(LoadSystemFont("arial.ttf"));
  // Load a config that only enables antialiasing for scalable fonts.
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(temp_dir_.path(),
      std::string(kFontconfigFileHeader) +
      kFontconfigMatchHeader +
      CreateFontconfigEditStanza("antialias", "bool", "false") +
      kFontconfigMatchFooter +
      kFontconfigMatchHeader +
      CreateFontconfigTestStanza("scalable", "eq", "bool", "true") +
      CreateFontconfigEditStanza("antialias", "bool", "true") +
      kFontconfigMatchFooter +
      kFontconfigFileFooter));

  // Check that we specifically ask how scalable fonts should be rendered.
  FontRenderParams params = GetCustomFontRenderParams(
      false, NULL, NULL, NULL, NULL, NULL);
  EXPECT_TRUE(params.antialiasing);
}

TEST_F(FontRenderParamsTest, UseBitmaps) {
  ASSERT_TRUE(LoadSystemFont("arial.ttf"));
  // Load a config that enables embedded bitmaps for fonts <= 10 pixels.
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(temp_dir_.path(),
      std::string(kFontconfigFileHeader) +
      kFontconfigMatchHeader +
      CreateFontconfigEditStanza("embeddedbitmap", "bool", "false") +
      kFontconfigMatchFooter +
      kFontconfigMatchHeader +
      CreateFontconfigTestStanza("pixelsize", "less_eq", "double", "10") +
      CreateFontconfigEditStanza("embeddedbitmap", "bool", "true") +
      kFontconfigMatchFooter +
      kFontconfigFileFooter));

  FontRenderParams params = GetCustomFontRenderParams(
      false, NULL, NULL, NULL, NULL, NULL);
  EXPECT_FALSE(params.use_bitmaps);

  const int pixel_size = 5;
  params = GetCustomFontRenderParams(
      false, NULL, &pixel_size, NULL, NULL, NULL);
  EXPECT_TRUE(params.use_bitmaps);
}

TEST_F(FontRenderParamsTest, OnlySetConfiguredValues) {
  // Configure the LinuxFontDelegate (which queries GtkSettings on desktop
  // Linux) to request subpixel rendering.
  FontRenderParams system_params;
  system_params.subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_RGB;
  test_font_delegate_.set_params(system_params);

  // Load a Fontconfig config that enables antialiasing but doesn't say anything
  // about subpixel rendering.
  ASSERT_TRUE(LoadSystemFont("arial.ttf"));
  ASSERT_TRUE(LoadConfigDataIntoFontconfig(temp_dir_.path(),
      std::string(kFontconfigFileHeader) +
      kFontconfigMatchHeader +
      CreateFontconfigEditStanza("antialias", "bool", "true") +
      kFontconfigMatchFooter +
      kFontconfigFileFooter));

  // The subpixel rendering setting from the delegate should make it through.
  FontRenderParams params = GetCustomFontRenderParams(
      false, NULL, NULL, NULL, NULL, NULL);
  EXPECT_EQ(system_params.subpixel_rendering, params.subpixel_rendering);
}

}  // namespace gfx
