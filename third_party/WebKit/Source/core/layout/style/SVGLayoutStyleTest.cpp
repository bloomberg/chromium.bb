// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/style/SVGLayoutStyle.h"

#include <gtest/gtest.h>

using namespace blink;

namespace {

// Ensures RefPtr values are compared by their values, not by pointers.
#define TEST_STYLE_REFPTR_VALUE_NO_DIFF(type, fieldName) \
    { \
        RefPtr<SVGLayoutStyle> svg1 = SVGLayoutStyle::create(); \
        RefPtr<SVGLayoutStyle> svg2 = SVGLayoutStyle::create(); \
        RefPtr<type> value1 = type::create(); \
        RefPtr<type> value2 = value1->copy(); \
        svg1->set##fieldName(value1); \
        svg2->set##fieldName(value2); \
        EXPECT_FALSE(svg1->diff(svg2.get()).hasDifference()); \
    }

// This is not very useful for fields directly stored by values, because they can only be
// compared by values. This macro mainly ensures that we update the comparisons and tests
// when we change some field to RefPtr in the future.
#define TEST_STYLE_VALUE_NO_DIFF(type, fieldName) \
    { \
        RefPtr<SVGLayoutStyle> svg1 = SVGLayoutStyle::create(); \
        RefPtr<SVGLayoutStyle> svg2 = SVGLayoutStyle::create(); \
        svg1->set##fieldName(SVGLayoutStyle::initial##fieldName()); \
        svg2->set##fieldName(SVGLayoutStyle::initial##fieldName()); \
        EXPECT_FALSE(svg1->diff(svg2.get()).hasDifference()); \
    }

TEST(SVGLayoutStyleTest, StrokeStyleShouldCompareValue)
{
    TEST_STYLE_VALUE_NO_DIFF(float, StrokeOpacity);
    TEST_STYLE_VALUE_NO_DIFF(float, StrokeMiterLimit);
    TEST_STYLE_VALUE_NO_DIFF(UnzoomedLength, StrokeWidth);
    TEST_STYLE_VALUE_NO_DIFF(Length, StrokeDashOffset);
    TEST_STYLE_REFPTR_VALUE_NO_DIFF(SVGDashArray, StrokeDashArray);

    {
        RefPtr<SVGLayoutStyle> svg1 = SVGLayoutStyle::create();
        RefPtr<SVGLayoutStyle> svg2 = SVGLayoutStyle::create();
        svg1->setStrokePaint(SVGLayoutStyle::initialStrokePaintType(), SVGLayoutStyle::initialStrokePaintColor(), SVGLayoutStyle::initialStrokePaintUri(), true, false);
        svg2->setStrokePaint(SVGLayoutStyle::initialStrokePaintType(), SVGLayoutStyle::initialStrokePaintColor(), SVGLayoutStyle::initialStrokePaintUri(), true, false);
        EXPECT_FALSE(svg1->diff(svg2.get()).hasDifference());
    }
    {
        RefPtr<SVGLayoutStyle> svg1 = SVGLayoutStyle::create();
        RefPtr<SVGLayoutStyle> svg2 = SVGLayoutStyle::create();
        svg1->setStrokePaint(SVGLayoutStyle::initialStrokePaintType(), SVGLayoutStyle::initialStrokePaintColor(), SVGLayoutStyle::initialStrokePaintUri(), false, true);
        svg2->setStrokePaint(SVGLayoutStyle::initialStrokePaintType(), SVGLayoutStyle::initialStrokePaintColor(), SVGLayoutStyle::initialStrokePaintUri(), false, true);
        EXPECT_FALSE(svg1->diff(svg2.get()).hasDifference());
    }
}

TEST(SVGLayoutStyleTest, MiscStyleShouldCompareValue)
{
    TEST_STYLE_VALUE_NO_DIFF(Color, FloodColor);
    TEST_STYLE_VALUE_NO_DIFF(float, FloodOpacity);
    TEST_STYLE_VALUE_NO_DIFF(Color, LightingColor);
    TEST_STYLE_VALUE_NO_DIFF(Length, BaselineShiftValue);
}

}
