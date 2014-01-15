/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "wtf/MathExtras.h"
#include "wtf/text/CString.h"
#include "wtf/text/WTFString.h"
#include <gtest/gtest.h>
#include <limits>

namespace {

TEST(WTF, StringCreationFromLiteral)
{
    String stringFromLiteral("Explicit construction syntax");
    ASSERT_EQ(strlen("Explicit construction syntax"), stringFromLiteral.length());
    ASSERT_TRUE(stringFromLiteral == "Explicit construction syntax");
    ASSERT_TRUE(stringFromLiteral.is8Bit());
    ASSERT_TRUE(String("Explicit construction syntax") == stringFromLiteral);
}

TEST(WTF, StringASCII)
{
    CString output;

    // Null String.
    output = String().ascii();
    ASSERT_STREQ("", output.data());

    // Empty String.
    output = emptyString().ascii();
    ASSERT_STREQ("", output.data());

    // Regular String.
    output = String("foobar").ascii();
    ASSERT_STREQ("foobar", output.data());
}

static void testNumberToStringECMAScript(double number, const char* reference)
{
    CString numberString = String::numberToStringECMAScript(number).latin1();
    ASSERT_STREQ(reference, numberString.data());
}

TEST(WTF, StringNumberToStringECMAScriptBoundaries)
{
    typedef std::numeric_limits<double> Limits;

    // Infinity.
    testNumberToStringECMAScript(Limits::infinity(), "Infinity");
    testNumberToStringECMAScript(-Limits::infinity(), "-Infinity");

    // NaN.
    testNumberToStringECMAScript(-Limits::quiet_NaN(), "NaN");

    // Zeros.
    testNumberToStringECMAScript(0, "0");
    testNumberToStringECMAScript(-0, "0");

    // Min-Max.
    testNumberToStringECMAScript(Limits::min(), "2.2250738585072014e-308");
    testNumberToStringECMAScript(Limits::max(), "1.7976931348623157e+308");
}

TEST(WTF, StringNumberToStringECMAScriptRegularNumbers)
{
    // Pi.
    testNumberToStringECMAScript(piDouble, "3.141592653589793");
    testNumberToStringECMAScript(piFloat, "3.1415927410125732");
    testNumberToStringECMAScript(piOverTwoDouble, "1.5707963267948966");
    testNumberToStringECMAScript(piOverTwoFloat, "1.5707963705062866");
    testNumberToStringECMAScript(piOverFourDouble, "0.7853981633974483");
    testNumberToStringECMAScript(piOverFourFloat, "0.7853981852531433");

    // e.
    const double e = 2.71828182845904523536028747135266249775724709369995;
    testNumberToStringECMAScript(e, "2.718281828459045");

    // c, speed of light in m/s.
    const double c = 299792458;
    testNumberToStringECMAScript(c, "299792458");

    // Golen ratio.
    const double phi = 1.6180339887498948482;
    testNumberToStringECMAScript(phi, "1.618033988749895");
}

TEST(WTF, StringReplaceWithLiteral)
{
    // Cases for 8Bit source.
    String testString = "1224";
    ASSERT_TRUE(testString.is8Bit());
    testString.replaceWithLiteral('2', "");
    ASSERT_STREQ("14", testString.utf8().data());

    testString = "1224";
    ASSERT_TRUE(testString.is8Bit());
    testString.replaceWithLiteral('2', "3");
    ASSERT_STREQ("1334", testString.utf8().data());

    testString = "1224";
    ASSERT_TRUE(testString.is8Bit());
    testString.replaceWithLiteral('2', "555");
    ASSERT_STREQ("15555554", testString.utf8().data());

    testString = "1224";
    ASSERT_TRUE(testString.is8Bit());
    testString.replaceWithLiteral('3', "NotFound");
    ASSERT_STREQ("1224", testString.utf8().data());

    // Cases for 16Bit source.
    testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.is8Bit());
    testString.replaceWithLiteral(UChar(0x00E9 /*U+00E9 is 'é'*/), "e");
    ASSERT_STREQ("resume", testString.utf8().data());

    testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.is8Bit());
    testString.replaceWithLiteral(UChar(0x00E9 /*U+00E9 is 'é'*/), "");
    ASSERT_STREQ("rsum", testString.utf8().data());

    testString = String::fromUTF8("résumé");
    ASSERT_FALSE(testString.is8Bit());
    testString.replaceWithLiteral('3', "NotFound");
    ASSERT_STREQ("résumé", testString.utf8().data());
}

TEST(WTF, StringComparisonOfSameStringVectors)
{
    Vector<String> stringVector;
    stringVector.append("one");
    stringVector.append("two");

    Vector<String> sameStringVector;
    sameStringVector.append("one");
    sameStringVector.append("two");

    ASSERT_EQ(stringVector, sameStringVector);
}

TEST(WTF, SimplifyWhiteSpace)
{
    String extraSpaces("  Hello  world  ");
    ASSERT_EQ(String("Hello world"), extraSpaces.simplifyWhiteSpace());
    ASSERT_EQ(String("  Hello  world  "), extraSpaces.simplifyWhiteSpace(WTF::DoNotStripWhiteSpace));

    String extraSpacesAndNewlines(" \nHello\n world\n ");
    ASSERT_EQ(String("Hello world"), extraSpacesAndNewlines.simplifyWhiteSpace());
    ASSERT_EQ(String("  Hello  world  "), extraSpacesAndNewlines.simplifyWhiteSpace(WTF::DoNotStripWhiteSpace));

    String extraSpacesAndTabs(" \nHello\t world\t ");
    ASSERT_EQ(String("Hello world"), extraSpacesAndTabs.simplifyWhiteSpace());
    ASSERT_EQ(String("  Hello  world  "), extraSpacesAndTabs.simplifyWhiteSpace(WTF::DoNotStripWhiteSpace));
}

struct CaseFoldingTestData {
    const char* sourceDescription;
    const char* source;
    const char** localeList;
    size_t localeListLength;
    const char* expected;
};

// \xC4\xB0 = U+0130 (capital dotted I)
// \xC4\xB1 = U+0131 (lowercase dotless I)
const char* turkicInput = "Isi\xC4\xB0 \xC4\xB0s\xC4\xB1I";
const char* greekInput = "ΟΔΌΣ Οδός Σο ΣΟ oΣ ΟΣ σ ἕξ";
const char* lithuanianInput = "I Ï J J̈ Į Į̈ Ì Í Ĩ xi̇̈ xj̇̈ xį̇̈ xi̇̀ xi̇́ xi̇̃ XI XÏ XJ XJ̈ XĮ XĮ̈";

const char* turkicLocales[] = {
    "tr", "tr-TR", "tr_TR", "tr@foo=bar", "tr-US", "TR", "tr-tr", "tR",
    "az", "az-AZ", "az_AZ", "az@foo=bar", "az-US", "Az", "AZ-AZ", };
const char* nonTurkicLocales[] = {
    "en", "en-US", "en_US", "en@foo=bar", "EN", "En",
    "ja", "el", "fil", "fi", "lt", };
const char* greekLocales[] = {
    "el", "el-GR", "el_GR", "el@foo=bar", "el-US", "EL", "el-gr", "eL",
};
const char* nonGreekLocales[] = {
    "en", "en-US", "en_US", "en@foo=bar", "EN", "En",
    "ja", "tr", "az", "fil", "fi", "lt", };
const char* lithuanianLocales[] = {
    "lt", "lt-LT", "lt_LT", "lt@foo=bar", "lt-US", "LT", "lt-lt", "lT",
};
// Should not have "tr" or "az" because "lt" and 'tr/az' rules conflict with each other.
const char* nonLithuanianLocales[] = {
    "en", "en-US", "en_US", "en@foo=bar", "EN", "En", "ja", "fil", "fi", "el", };

TEST(WTF, StringToUpperLocale)
{
    CaseFoldingTestData testDataList[] = {
        {
            "Turkic input",
            turkicInput,
            turkicLocales,
            sizeof(turkicLocales) / sizeof(const char*),
            "IS\xC4\xB0\xC4\xB0 \xC4\xB0SII",
        }, {
            "Turkic input",
            turkicInput,
            nonTurkicLocales,
            sizeof(nonTurkicLocales) / sizeof(const char*),
            "ISI\xC4\xB0 \xC4\xB0SII",
        }, {
            "Greek input",
            greekInput,
            greekLocales,
            sizeof(greekLocales) / sizeof(const char*),
            "ΟΔΟΣ ΟΔΟΣ ΣΟ ΣΟ OΣ ΟΣ Σ ΕΞ",
        }, {
            "Greek input",
            greekInput,
            nonGreekLocales,
            sizeof(nonGreekLocales) / sizeof(const char*),
            "ΟΔΌΣ ΟΔΌΣ ΣΟ ΣΟ OΣ ΟΣ Σ ἝΞ",
        }, {
            "Lithuanian input",
            lithuanianInput,
            lithuanianLocales,
            sizeof(lithuanianLocales) / sizeof(const char*),
            "I Ï J J̈ Į Į̈ Ì Í Ĩ XÏ XJ̈ XĮ̈ XÌ XÍ XĨ XI XÏ XJ XJ̈ XĮ XĮ̈",
        }, {
            "Lithuanian input",
            lithuanianInput,
            nonLithuanianLocales,
            sizeof(nonLithuanianLocales) / sizeof(const char*),
            "I Ï J J̈ Į Į̈ Ì Í Ĩ Xİ̈ XJ̇̈ XĮ̇̈ Xİ̀ Xİ́ Xİ̃ XI XÏ XJ XJ̈ XĮ XĮ̈",
        },
    };

    for (size_t i = 0; i < sizeof(testDataList) / sizeof(testDataList[0]); ++i) {
        const char* expected = testDataList[i].expected;
        String source = String::fromUTF8(testDataList[i].source);
        for (size_t j = 0; j < testDataList[i].localeListLength; ++j) {
            const char* locale = testDataList[i].localeList[j];
            EXPECT_STREQ(expected, source.upper(locale).utf8().data()) << testDataList[i].sourceDescription << "; locale=" << locale;
        }
    }
}

TEST(WTF, StringToLowerLocale)
{
    CaseFoldingTestData testDataList[] = {
        {
            "Turkic input",
            turkicInput,
            turkicLocales,
            sizeof(turkicLocales) / sizeof(const char*),
            "ısii isıı",
        }, {
            "Turkic input",
            turkicInput,
            nonTurkicLocales,
            sizeof(nonTurkicLocales) / sizeof(const char*),
            // U+0130 is lowercased to U+0069 followed by U+0307
            "isii\xCC\x87 i\xCC\x87s\xC4\xB1i",
        }, {
            "Greek input",
            greekInput,
            greekLocales,
            sizeof(greekLocales) / sizeof(const char*),
            "οδός οδός σο σο oς ος σ ἕξ",
        }, {
            "Greek input",
            greekInput,
            nonGreekLocales,
            sizeof(greekLocales) / sizeof(const char*),
            "οδός οδός σο σο oς ος σ ἕξ",
        }, {
            "Lithuanian input",
            lithuanianInput,
            lithuanianLocales,
            sizeof(lithuanianLocales) / sizeof(const char*),
            "i ï j j̇̈ į į̇̈ i̇̀ i̇́ i̇̃ xi̇̈ xj̇̈ xį̇̈ xi̇̀ xi̇́ xi̇̃ xi xï xj xj̇̈ xį xį̇̈",
        }, {
            "Lithuanian input",
            lithuanianInput,
            nonLithuanianLocales,
            sizeof(nonLithuanianLocales) / sizeof(const char*),
            "i ï j j̈ į į̈ ì í ĩ xi̇̈ xj̇̈ xį̇̈ xi̇̀ xi̇́ xi̇̃ xi xï xj xj̈ xį xį̈",
        },
    };

    for (size_t i = 0; i < sizeof(testDataList) / sizeof(testDataList[0]); ++i) {
        const char* expected = testDataList[i].expected;
        String source = String::fromUTF8(testDataList[i].source);
        for (size_t j = 0; j < testDataList[i].localeListLength; ++j) {
            const char* locale = testDataList[i].localeList[j];
            EXPECT_STREQ(expected, source.lower(locale).utf8().data()) << testDataList[i].sourceDescription << "; locale=" << locale;
        }
    }
}

} // namespace
