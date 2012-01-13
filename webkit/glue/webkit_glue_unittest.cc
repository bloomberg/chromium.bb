// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include <string>

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

TEST(WebkitGlueTest, DecodeImageFail) {
  std::string data("not an image");
  SkBitmap image;
  EXPECT_FALSE(webkit_glue::DecodeImage(data, &image));
  EXPECT_TRUE(image.isNull());
}

TEST(WebkitGlueTest, DecodeImage) {
  std::string data("GIF87a\x02\x00\x02\x00\xa1\x04\x00\x00\x00\x00\x00\x00\xff"
                   "\xff\x00\x00\x00\xff\x00,\x00\x00\x00\x00\x02\x00\x02\x00"
                   "\x00\x02\x03\x84\x16\x05\x00;", 42);
  EXPECT_EQ(42u, data.size());
  SkBitmap image;
  EXPECT_TRUE(webkit_glue::DecodeImage(data, &image));
  EXPECT_FALSE(image.isNull());
  EXPECT_EQ(2, image.width());
  EXPECT_EQ(2, image.height());
  EXPECT_EQ(SkBitmap::kARGB_8888_Config, image.config());
  image.lockPixels();
  EXPECT_EQ(SK_ColorBLACK, *image.getAddr32(0, 0));
  EXPECT_EQ(SK_ColorRED, *image.getAddr32(1, 0));
  EXPECT_EQ(SK_ColorGREEN, *image.getAddr32(0, 1));
  EXPECT_EQ(SK_ColorBLUE, *image.getAddr32(1, 1));
  image.unlockPixels();
}

class WebkitGlueUserAgentTest : public TestShellTest {
};

bool IsSpoofedUserAgent(const std::string& user_agent) {
  return user_agent.find("TestShell") == std::string::npos;
}

TEST_F(WebkitGlueUserAgentTest, UserAgentSpoofingHack) {
  const char* urls[] = {
      "http://wwww.google.com",
      "http://www.microsoft.com/getsilverlight",
      "http://headlines.yahoo.co.jp/videonews/",
      "http://downloads.yahoo.co.jp/docs/silverlight/",
      "http://gyao.yahoo.co.jp/",
      "http://weather.yahoo.co.jp/weather/zoomradar/",
      "http://promotion.shopping.yahoo.co.jp/"};
#if defined(OS_MACOSX)
  bool spoofed[] = {
      false,
      true,
      true,
      true,
      true,
      false,
      false};
#elif defined(OS_WIN)
  bool spoofed[] = {
      false,
      false,
      true,
      false,
      false,
      true,
      true};
#else
  bool spoofed[] = {
      false,
      false,
      false,
      false,
      false,
      false,
      false};
#endif

  ASSERT_EQ(arraysize(urls), arraysize(spoofed));

  for (size_t i = 0; i < arraysize(urls); i++) {
    EXPECT_EQ(spoofed[i],
              IsSpoofedUserAgent(webkit_glue::GetUserAgent(GURL(urls[i]))));
  }
}

}  // namespace
