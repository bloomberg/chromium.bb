// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/webui/web_ui_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(WebUIUtilTest, ParsePathAndScale) {
  std::string path;

  float factor = 0;
  GURL url("http://some/random/username@email/and/more");
  webui::ParsePathAndScale(url, &path, &factor);
  EXPECT_EQ("random/username@email/and/more", path);
  EXPECT_EQ(1.0f, factor);

  factor = 0;
  GURL url2("http://some/random/username/and/more");
  webui::ParsePathAndScale(url2, &path, &factor);
  EXPECT_EQ("random/username/and/more", path);
  EXPECT_EQ(1.0f, factor);

  factor = 0;
  GURL url3("http://some/random/username/and/more@2ax");
  webui::ParsePathAndScale(url3, &path, &factor);
  EXPECT_EQ("random/username/and/more@2ax", path);
  EXPECT_EQ(1.0f, factor);

  factor = 0;
  GURL url4("http://some/random/username/and/more@x");
  webui::ParsePathAndScale(url4, &path, &factor);
  EXPECT_EQ("random/username/and/more@x", path);
  EXPECT_EQ(1.0f, factor);

  factor = 0;
  GURL url5("http://some/random/username@email/and/more@2x");
  webui::ParsePathAndScale(url5, &path, &factor);
  EXPECT_EQ("random/username@email/and/more", path);
  EXPECT_EQ(2.0f, factor);

  factor = 0;
  GURL url6("http://some/random/username/and/more@1.4x");
  webui::ParsePathAndScale(url6, &path, &factor);
  EXPECT_EQ("random/username/and/more", path);
  EXPECT_EQ(1.4f, factor);

  factor = 0;
  GURL url7("http://some/random/username/and/more@1.3x");
  webui::ParsePathAndScale(url7, &path, &factor);
  EXPECT_EQ("random/username/and/more", path);
  EXPECT_EQ(1.3f, factor);
}

TEST(WebUIUtilTest, ParsePathAndImageSpec) {
  std::string path;

  float factor = 0;
  int index = -2;
  GURL url("http://[::192.9.5.5]/some/random/username@email/and/more");
  webui::ParsePathAndImageSpec(url, &path, &factor, &index);
  EXPECT_EQ("some/random/username@email/and/more", path);
  EXPECT_EQ(1.0f, factor);
  EXPECT_EQ(-1, index);

  factor = 0;
  index = -2;
  GURL url2("http://host/some/random/username/and/more");
  webui::ParsePathAndImageSpec(url2, &path, &factor, &index);
  EXPECT_EQ("some/random/username/and/more", path);
  EXPECT_EQ(1.0f, factor);
  EXPECT_EQ(-1, index);

  factor = 0;
  index = -2;
  GURL url3("http://host/some/random/username/and/more[0]@2x");
  webui::ParsePathAndImageSpec(url3, &path, &factor, &index);
  EXPECT_EQ("some/random/username/and/more", path);
  EXPECT_EQ(2.0f, factor);
  EXPECT_EQ(0, index);

  factor = 0;
  index = -2;
  GURL url4("http://[::192.9.5.5]/some/random/username@email/and/more[1]@1.5x");
  webui::ParsePathAndImageSpec(url4, &path, &factor, &index);
  EXPECT_EQ("some/random/username@email/and/more", path);
  EXPECT_EQ(1.5f, factor);
  EXPECT_EQ(1, index);
}

TEST(WebUIUtilTest, GetPngDataUrl_Basic) {
  // The input doesn't have to be a valid image.
  std::vector<unsigned char> in = {1, 2, 3, 4};
  std::string out = webui::GetPngDataUrl(in.data(), in.size());
  EXPECT_EQ("data:image/png;base64,AQIDBA==", out);
}

TEST(WebUIUtilTest, GetPngDataUrl_EmptyInput) {
  std::vector<unsigned char> in;
  webui::GetPngDataUrl(in.data(), in.size());
  // No crash.
}
