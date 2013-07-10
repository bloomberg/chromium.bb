/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Tests for the Font class.

#include "config.h"

#include "core/platform/graphics/Font.h"

#include <gtest/gtest.h>

namespace WebCore {

TEST(FontTest, TestCharacterRangeCodePath1)
{
    static UChar c1[] = { 0x0 };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x2E4 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c3[] = { 0x2E5 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x2E8 };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x2E9 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c6[] = { 0x2EA };
    codePath = Font::characterRangeCodePath(c6, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath2)
{
    static UChar c1[] = { 0x2FF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x300 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x330 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x36F };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x370 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath3)
{
    static UChar c1[] = { 0x0590 };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x0591 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x05A0 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x05BD };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x05BE };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c6[] = { 0x05BF };
    codePath = Font::characterRangeCodePath(c6, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c7[] = { 0x05C0 };
    codePath = Font::characterRangeCodePath(c7, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c8[] = { 0x05CF };
    codePath = Font::characterRangeCodePath(c8, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c9[] = { 0x05D0 };
    codePath = Font::characterRangeCodePath(c9, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath4)
{
    static UChar c1[] = { 0x05FF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x0600 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x0700 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x109F };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x10A0 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath5)
{
    static UChar c1[] = { 0x10FF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x1100 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x11A0 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x11FF };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x1200 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath6)
{
    static UChar c1[] = { 0x135C };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x135D };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x135E };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x135F };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x1360 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath7)
{
    static UChar c1[] = { 0x16FF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x1700 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x1800 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x18AF };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x18B0 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath8)
{
    static UChar c1[] = { 0x18FF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x1900 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x1940 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x194F };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x1950 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath9)
{
    static UChar c1[] = { 0x197F };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x1980 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x19D0 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x19DF };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x19E0 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath10)
{
    static UChar c1[] = { 0x19FF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x1A00 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x1C00 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x1CFF };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x1D00 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath11)
{
    static UChar c1[] = { 0x1DBF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x1DC0 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x1DD0 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x1DFF };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x1E00 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::SimpleWithGlyphOverflow, codePath);

    static UChar c6[] = { 0x2000 };
    codePath = Font::characterRangeCodePath(c6, 1);
    EXPECT_EQ(Font::SimpleWithGlyphOverflow, codePath);

    static UChar c7[] = { 0x2001 };
    codePath = Font::characterRangeCodePath(c7, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath12)
{
    static UChar c1[] = { 0x20CF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x20D0 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x20F0 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x20FF };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x2100 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath13)
{
    static UChar c1[] = { 0x2CED };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x2CEF };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x2CF0 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x2CF1 };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x2CF2 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath14)
{
    static UChar c1[] = { 0x3029 };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0x302A };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0x302C };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0x302F };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0x3030 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath15)
{
    static UChar c1[] = { 0xA67B };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0xA67C };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0xA67D };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0xA67E };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath16)
{
    static UChar c1[] = { 0xA6E9 };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0xA6F0 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0xA6F1 };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0xA6F2 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath17)
{
    static UChar c1[] = { 0xA7FF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0xA800 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0xAA00 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0xABFF };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0xAC00 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath18)
{
    static UChar c1[] = { 0xD7AF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0xD7B0 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0xD7F0 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0xD7FF };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0xD800 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath19)
{
    static UChar c1[] = { 0xFDFF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0xFE00 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0xFE05 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0xFE0F };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0xFE10 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePath20)
{
    static UChar c1[] = { 0xFD1F };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 1);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0xFE20 };
    codePath = Font::characterRangeCodePath(c2, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c3[] = { 0xFE28 };
    codePath = Font::characterRangeCodePath(c3, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c4[] = { 0xFE2F };
    codePath = Font::characterRangeCodePath(c4, 1);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c5[] = { 0xFE30 };
    codePath = Font::characterRangeCodePath(c5, 1);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePathSurrogate1)
{
    /* To be surrogate ... */
    /* 1st character must be 0xD800 .. 0xDBFF */
    /* 2nd character must be 0xdc00 .. 0xdfff */

    /* The following 5 should all be Simple because they are not surrogate. */
    static UChar c1[] = { 0xD800, 0xDBFE };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 2);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c2[] = { 0xD800, 0xE000 };
    codePath = Font::characterRangeCodePath(c2, 2);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c3[] = { 0xDBFF, 0xDBFE };
    codePath = Font::characterRangeCodePath(c3, 2);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c4[] = { 0xDBFF, 0xE000 };
    codePath = Font::characterRangeCodePath(c4, 2);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c5[] = { 0xDC00, 0xDBFF };
    codePath = Font::characterRangeCodePath(c5, 2);
    EXPECT_EQ(Font::Simple, codePath);

    /* To be Complex, the Supplementary Character must be in either */
    /* U+1F1E6 through U+1F1FF or U+E0100 through U+E01EF. */
    /* That is, a lead of 0xD83C with trail 0xDDE6 .. 0xDDFF or */
    /* a lead of 0xDB40 with trail 0xDD00 .. 0xDDEF. */
    static UChar c6[] = { 0xD83C, 0xDDE5 };
    codePath = Font::characterRangeCodePath(c6, 2);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c7[] = { 0xD83C, 0xDDE6 };
    codePath = Font::characterRangeCodePath(c7, 2);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c8[] = { 0xD83C, 0xDDF0 };
    codePath = Font::characterRangeCodePath(c8, 2);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c9[] = { 0xD83C, 0xDDFF };
    codePath = Font::characterRangeCodePath(c9, 2);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c10[] = { 0xD83C, 0xDE00 };
    codePath = Font::characterRangeCodePath(c10, 2);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c11[] = { 0xDB40, 0xDCFF };
    codePath = Font::characterRangeCodePath(c11, 2);
    EXPECT_EQ(Font::Simple, codePath);

    static UChar c12[] = { 0xDB40, 0xDD00 };
    codePath = Font::characterRangeCodePath(c12, 2);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c13[] = { 0xDB40, 0xDDED };
    codePath = Font::characterRangeCodePath(c13, 2);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c14[] = { 0xDB40, 0xDDEF };
    codePath = Font::characterRangeCodePath(c14, 2);
    EXPECT_EQ(Font::Complex, codePath);

    static UChar c15[] = { 0xDB40, 0xDDF0 };
    codePath = Font::characterRangeCodePath(c15, 2);
    EXPECT_EQ(Font::Simple, codePath);
}

TEST(FontTest, TestCharacterRangeCodePathString)
{
    // Simple-Simple is still simple
    static UChar c1[] = { 0x2FF, 0x2FF };
    Font::CodePath codePath = Font::characterRangeCodePath(c1, 2);
    EXPECT_EQ(Font::Simple, codePath);

    // Complex-Simple is Complex
    static UChar c2[] = { 0x300, 0x2FF };
    codePath = Font::characterRangeCodePath(c2, 2);
    EXPECT_EQ(Font::Complex, codePath);

    // Simple-Complex is Complex
    static UChar c3[] = { 0x2FF, 0x330 };
    codePath = Font::characterRangeCodePath(c3, 2);
    EXPECT_EQ(Font::Complex, codePath);

    // Complex-Complex is Complex
    static UChar c4[] = { 0x36F, 0x330 };
    codePath = Font::characterRangeCodePath(c4, 2);
    EXPECT_EQ(Font::Complex, codePath);

    // SimpleWithGlyphOverflow-Simple is SimpleWithGlyphOverflow
    static UChar c5[] = { 0x1E00, 0x2FF };
    codePath = Font::characterRangeCodePath(c5, 2);
    EXPECT_EQ(Font::SimpleWithGlyphOverflow, codePath);

    // Simple-SimpleWithGlyphOverflow is SimpleWithGlyphOverflow
    static UChar c6[] = { 0x2FF, 0x2000 };
    codePath = Font::characterRangeCodePath(c6, 2);
    EXPECT_EQ(Font::SimpleWithGlyphOverflow, codePath);

    // SimpleWithGlyphOverflow-Complex is Complex
    static UChar c7[] = { 0x1E00, 0x330 };
    codePath = Font::characterRangeCodePath(c7, 2);
    EXPECT_EQ(Font::Complex, codePath);

    // Complex-SimpleWithGlyphOverflow is Complex
    static UChar c8[] = { 0x330, 0x2000 };
    codePath = Font::characterRangeCodePath(c8, 2);
    EXPECT_EQ(Font::Complex, codePath);

    // Surrogate-Complex is Complex
    static UChar c9[] = { 0xD83C, 0xDDE5, 0x330 };
    codePath = Font::characterRangeCodePath(c9, 3);
    EXPECT_EQ(Font::Complex, codePath);

    // Complex-Surrogate is Complex
    static UChar c10[] = { 0x330, 0xD83C, 0xDDE5 };
    codePath = Font::characterRangeCodePath(c10, 3);
    EXPECT_EQ(Font::Complex, codePath);
}


} // namespace WebCore

