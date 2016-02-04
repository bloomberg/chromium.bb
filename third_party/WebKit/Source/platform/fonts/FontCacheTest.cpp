// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/FontCache.h"

#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class EmptyPlatform : public TestingPlatformSupport {
public:
    EmptyPlatform() {}
    ~EmptyPlatform() override {}
};

TEST(FontCache, getLastResortFallbackFont)
{
    FontCache* fontCache = FontCache::fontCache();
    ASSERT_TRUE(fontCache);

    EmptyPlatform platform;

    FontDescription fontDescription;
    fontDescription.setGenericFamily(FontDescription::StandardFamily);
    RefPtr<SimpleFontData> fontData = fontCache->getLastResortFallbackFont(fontDescription, Retain);
    EXPECT_TRUE(fontData);

    fontDescription.setGenericFamily(FontDescription::SansSerifFamily);
    fontData = fontCache->getLastResortFallbackFont(fontDescription, Retain);
    EXPECT_TRUE(fontData);
}

class FontCacheAvailabilityTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_fontCache = FontCache::fontCache();
        ASSERT_TRUE(m_fontCache);
        m_fontDescription.setGenericFamily(FontDescription::StandardFamily);
    }

    void TearDown() override
    {
        EXPECT_TRUE(m_availableFonts);
        EXPECT_GE(m_availableFonts->size(), 1u);
    }

    FontCache* m_fontCache;
    EmptyPlatform m_emptyPlatform;
    FontDescription m_fontDescription;
    const Vector<AtomicString>* m_availableFonts;
};

TEST_F(FontCacheAvailabilityTest, availableSymbolsFonts)
{
    m_availableFonts = m_fontCache->fontListForFallbackPriority(
        m_fontDescription,
        FontFallbackPriority::Symbols);
}

TEST_F(FontCacheAvailabilityTest, availableMathFonts)
{
    m_availableFonts = m_fontCache->fontListForFallbackPriority(
        m_fontDescription,
        FontFallbackPriority::Math);
}

TEST_F(FontCacheAvailabilityTest, availableEmojiTextFonts)
{
    m_availableFonts = m_fontCache->fontListForFallbackPriority(
        m_fontDescription,
        FontFallbackPriority::EmojiText);
}

TEST_F(FontCacheAvailabilityTest, availableEmojiEmojiFonts)
{
    m_availableFonts = m_fontCache->fontListForFallbackPriority(
        m_fontDescription,
        FontFallbackPriority::EmojiEmoji);
}

} // namespace blink
