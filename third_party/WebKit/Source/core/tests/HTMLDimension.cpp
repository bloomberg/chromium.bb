/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/HTMLDimension.h"

#include "wtf/text/WTFString.h"
#include <gtest/gtest.h>

namespace WebCore {

// This assertion-prettify function needs to be in the WebCore namespace.
void PrintTo(const Length& length, ::std::ostream* os)
{
    *os << "Length => lengthType: " << length.type() << ", value=" << length.value();
}

}

using namespace WebCore;

namespace {

TEST(WebCoreHTMLDimension, parseListOfDimensionsEmptyString)
{
    Vector<Length> result = parseListOfDimensions(String(""));
    ASSERT_EQ(Vector<Length>(), result);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsNoNumberAbsolute)
{
    Vector<Length> result = parseListOfDimensions(String(" \t"));
    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(0, Relative), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsNoNumberPercent)
{
    Vector<Length> result = parseListOfDimensions(String(" \t%"));
    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(0, Percent), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsNoNumberRelative)
{
    Vector<Length> result = parseListOfDimensions(String("\t *"));
    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(0, Relative), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsSingleAbsolute)
{
    Vector<Length> result = parseListOfDimensions(String("10"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(10, Fixed), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsSinglePercentageWithSpaces)
{
    Vector<Length> result = parseListOfDimensions(String("50  %"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(50, Percent), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsSingleRelative)
{
    Vector<Length> result = parseListOfDimensions(String("25*"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(25, Relative), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsDoubleAbsolute)
{
    Vector<Length> result = parseListOfDimensions(String("10.054"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(10.054, Fixed), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsLeadingSpaceAbsolute)
{
    Vector<Length> result = parseListOfDimensions(String("\t \t 10"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(10, Fixed), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsLeadingSpaceRelative)
{
    Vector<Length> result = parseListOfDimensions(String(" \r25*"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(25, Relative), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsLeadingSpacePercentage)
{
    Vector<Length> result = parseListOfDimensions(String("\n 25%"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(25, Percent), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsDoublePercentage)
{
    Vector<Length> result = parseListOfDimensions(String("10.054%"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(10.054, Percent), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsDoubleRelative)
{
    Vector<Length> result = parseListOfDimensions(String("10.054*"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(10.054, Relative), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsSpacesInIntegerDoubleAbsolute)
{
    Vector<Length> result = parseListOfDimensions(String("1\n0 .025%"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(1, Fixed), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsSpacesInIntegerDoublePercent)
{
    Vector<Length> result = parseListOfDimensions(String("1\n0 .025%"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(1, Fixed), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsSpacesInIntegerDoubleRelative)
{
    Vector<Length> result = parseListOfDimensions(String("1\n0 .025*"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(1, Fixed), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsSpacesInFractionAfterDotDoublePercent)
{
    Vector<Length> result = parseListOfDimensions(String("10.  0 25%"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(10.025, Percent), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsSpacesInFractionAfterDigitDoublePercent)
{
    Vector<Length> result = parseListOfDimensions(String("10.05\r25%"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(10.0525, Percent), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsTrailingComma)
{
    Vector<Length> result = parseListOfDimensions(String("10,"));

    ASSERT_EQ(1U, result.size());
    ASSERT_EQ(Length(10, Fixed), result[0]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsTwoDimensions)
{
    Vector<Length> result = parseListOfDimensions(String("10*,25 %"));

    ASSERT_EQ(2U, result.size());
    ASSERT_EQ(Length(10, Relative), result[0]);
    ASSERT_EQ(Length(25, Percent), result[1]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsMultipleDimensionsWithSpaces)
{
    Vector<Length> result = parseListOfDimensions(String("10   *   ,\t25 , 10.05\n5%"));

    ASSERT_EQ(3U, result.size());
    ASSERT_EQ(Length(10, Relative), result[0]);
    ASSERT_EQ(Length(25, Fixed), result[1]);
    ASSERT_EQ(Length(10.055, Percent), result[2]);
}

TEST(WebCoreHTMLDimension, parseListOfDimensionsMultipleDimensionsWithOneEmpty)
{
    Vector<Length> result = parseListOfDimensions(String("2*,,8.%"));

    ASSERT_EQ(3U, result.size());
    ASSERT_EQ(Length(2, Relative), result[0]);
    ASSERT_EQ(Length(0, Relative), result[1]);
    ASSERT_EQ(Length(8., Percent), result[2]);
}

}
