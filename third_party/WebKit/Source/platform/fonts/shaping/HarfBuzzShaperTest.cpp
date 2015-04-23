// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/GlyphPage.h"
#include "platform/text/TextRun.h"
#include "wtf/Vector.h"
#include <gtest/gtest.h>
#include <unicode/uscript.h>

namespace {

using namespace blink;
using namespace WTF;

class HarfBuzzShaperTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        fontDescription.setComputedSize(12.0);
        font = new Font(fontDescription);
        font->update(nullptr);
    }

    virtual void TearDown()
    {
        delete font;
    }

    FontCachePurgePreventer fontCachePurgePreventer;
    FontDescription fontDescription;
    Font* font;
    unsigned startIndex = 0;
    unsigned numGlyphs = 0;
    hb_script_t script = HB_SCRIPT_INVALID;
};


TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsLatin)
{
    TextRun latinCommon("ABC DEF.", 8);
    HarfBuzzShaper shaper(font, latinCommon);
    shaper.shape();

    ASSERT_EQ(shaper.numberOfRunsForTesting(), 1u);
    ASSERT_EQ(shaper.runInfoForTesting(0, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 0u);
    ASSERT_EQ(numGlyphs, 8u);
    ASSERT_EQ(script, HB_SCRIPT_LATIN);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsLeadingCommon)
{
    TextRun leadingCommon("... test", 8);
    HarfBuzzShaper shaper(font, leadingCommon);
    shaper.shape();

    ASSERT_EQ(shaper.numberOfRunsForTesting(), 1u);
    ASSERT_EQ(shaper.runInfoForTesting(0, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 0u);
    ASSERT_EQ(numGlyphs, 8u);
    ASSERT_EQ(script, HB_SCRIPT_LATIN);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsDevanagariCommon)
{
    UChar devanagariCommonString[] = { 0x915, 0x94d, 0x930, 0x28, 0x20, 0x29 };
    TextRun devanagariCommonLatin(devanagariCommonString, 6);
    HarfBuzzShaper shaper(font, devanagariCommonLatin);
    shaper.shape();

    ASSERT_EQ(shaper.numberOfRunsForTesting(), 2u);
    ASSERT_EQ(shaper.runInfoForTesting(0, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 0u);
    ASSERT_EQ(numGlyphs, 1u);
    ASSERT_EQ(script, HB_SCRIPT_DEVANAGARI);

    ASSERT_EQ(shaper.runInfoForTesting(1, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 3u);
    ASSERT_EQ(numGlyphs, 3u);
    ASSERT_EQ(script, HB_SCRIPT_DEVANAGARI);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsDevanagariCommonLatinCommon)
{
    UChar devanagariCommonLatinString[] = { 0x915, 0x94d, 0x930, 0x20, 0x61, 0x62, 0x2E };
    TextRun devanagariCommonLatin(devanagariCommonLatinString, 7);
    HarfBuzzShaper shaper(font, devanagariCommonLatin);
    shaper.shape();

    ASSERT_EQ(shaper.numberOfRunsForTesting(), 3u);
    ASSERT_EQ(shaper.runInfoForTesting(0, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 0u);
    ASSERT_EQ(numGlyphs, 1u);
    ASSERT_EQ(script, HB_SCRIPT_DEVANAGARI);

    ASSERT_EQ(shaper.runInfoForTesting(1, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 3u);
    ASSERT_EQ(numGlyphs, 1u);
    ASSERT_EQ(script, HB_SCRIPT_DEVANAGARI);

    ASSERT_EQ(shaper.runInfoForTesting(2, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 4u);
    ASSERT_EQ(numGlyphs, 3u);
    ASSERT_EQ(script, HB_SCRIPT_LATIN);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabicThaiHanLatin)
{
    UChar mixedString[] = { 0x628, 0x64A, 0x629, 0xE20, 0x65E5, 0x62 };
    TextRun mixed(mixedString, 6);
    HarfBuzzShaper shaper(font, mixed);
    shaper.shape();

    ASSERT_EQ(shaper.numberOfRunsForTesting(), 4u);
    ASSERT_EQ(shaper.runInfoForTesting(0, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 0u);
    ASSERT_EQ(numGlyphs, 3u);
    ASSERT_EQ(script, HB_SCRIPT_ARABIC);

    ASSERT_EQ(shaper.runInfoForTesting(1, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 3u);
    ASSERT_EQ(numGlyphs, 1u);
    ASSERT_EQ(script, HB_SCRIPT_THAI);

    ASSERT_EQ(shaper.runInfoForTesting(2, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 4u);
    ASSERT_EQ(numGlyphs, 1u);
    ASSERT_EQ(script, HB_SCRIPT_HAN);

    ASSERT_EQ(shaper.runInfoForTesting(3, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 5u);
    ASSERT_EQ(numGlyphs, 1u);
    ASSERT_EQ(script, HB_SCRIPT_LATIN);
}

TEST_F(HarfBuzzShaperTest, ResolveCandidateRunsArabic)
{
    UChar arabicString[] = { 0x628, 0x64A, 0x629 };
    TextRun arabic(arabicString, 3);
    HarfBuzzShaper shaper(font, arabic);
    shaper.shape();

    ASSERT_EQ(shaper.numberOfRunsForTesting(), 1u);
    ASSERT_EQ(shaper.runInfoForTesting(0, startIndex, numGlyphs, script), true);
    ASSERT_EQ(startIndex, 0u);
    ASSERT_EQ(numGlyphs, 3u);
    ASSERT_EQ(script, HB_SCRIPT_ARABIC);
}

}
