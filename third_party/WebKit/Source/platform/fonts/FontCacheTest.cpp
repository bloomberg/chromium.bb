// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontCache.h"

#include "build/build_config.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FontCache, getLastResortFallbackFont) {
  FontCache* font_cache = FontCache::GetFontCache();
  ASSERT_TRUE(font_cache);

  FontDescription font_description;
  font_description.SetGenericFamily(FontDescription::kStandardFamily);
  RefPtr<SimpleFontData> font_data =
      font_cache->GetLastResortFallbackFont(font_description, kRetain);
  EXPECT_TRUE(font_data);

  font_description.SetGenericFamily(FontDescription::kSansSerifFamily);
  font_data = font_cache->GetLastResortFallbackFont(font_description, kRetain);
  EXPECT_TRUE(font_data);
}

TEST(FontCache, firstAvailableOrFirst) {
  EXPECT_TRUE(FontCache::FirstAvailableOrFirst("").IsEmpty());
  EXPECT_TRUE(FontCache::FirstAvailableOrFirst(String()).IsEmpty());

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("Arial"));
  EXPECT_EQ("not exist", FontCache::FirstAvailableOrFirst("not exist"));

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("Arial, not exist"));
  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst("not exist, Arial"));
  EXPECT_EQ("Arial",
            FontCache::FirstAvailableOrFirst("not exist, Arial, not exist"));

  EXPECT_EQ("not exist",
            FontCache::FirstAvailableOrFirst("not exist, not exist 2"));

  EXPECT_EQ("Arial", FontCache::FirstAvailableOrFirst(", not exist, Arial"));
  EXPECT_EQ("not exist",
            FontCache::FirstAvailableOrFirst(", not exist, not exist"));
}

#if !defined(OS_MACOSX)
TEST(FontCache, systemFont) {
  FontCache::SystemFontFamily();
  // Test the function does not crash. Return value varies by system and config.
}
#endif

}  // namespace blink
