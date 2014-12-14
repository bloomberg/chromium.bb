// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/style/SVGRenderStyle.h"

#include "core/rendering/ClipPathOperation.h"
#include "core/rendering/style/ShapeValue.h"

#include <gtest/gtest.h>

using namespace blink;

namespace {

// Ensures RefPtr values are compared by their values, not by pointers.
#define TEST_STYLE_REFPTR_VALUE_NO_DIFF(type, fieldName) \
    { \
        RefPtr<SVGRenderStyle> svg1 = SVGRenderStyle::create(); \
        RefPtr<SVGRenderStyle> svg2 = SVGRenderStyle::create(); \
        RefPtrWillBePersistent<type> value1 = type::create(); \
        RefPtrWillBePersistent<type> value2 = value1->clone(); \
        svg1->set##fieldName(value1); \
        svg2->set##fieldName(value2); \
        EXPECT_FALSE(svg1->diff(svg2.get()).hasDifference()); \
    }

// This is not very useful for fields directly stored by values, because they can only be
// compared by values. This macro mainly ensures that we update the comparisons and tests
// when we change some field to RefPtr in the future.
#define TEST_STYLE_VALUE_NO_DIFF(type, fieldName) \
    { \
        RefPtr<SVGRenderStyle> svg1 = SVGRenderStyle::create(); \
        RefPtr<SVGRenderStyle> svg2 = SVGRenderStyle::create(); \
        svg1->set##fieldName(SVGRenderStyle::initial##fieldName()); \
        svg2->set##fieldName(SVGRenderStyle::initial##fieldName()); \
        EXPECT_FALSE(svg1->diff(svg2.get()).hasDifference()); \
    }

TEST(SVGRenderStyleTest, StrokeStyleShouldCompareValue)
{
    TEST_STYLE_VALUE_NO_DIFF(float, StrokeOpacity);
    TEST_STYLE_VALUE_NO_DIFF(float, StrokeMiterLimit);
    TEST_STYLE_REFPTR_VALUE_NO_DIFF(SVGLength, StrokeWidth);
    TEST_STYLE_REFPTR_VALUE_NO_DIFF(SVGLength, StrokeDashOffset);
    TEST_STYLE_REFPTR_VALUE_NO_DIFF(SVGLengthList, StrokeDashArray);

    {
        RefPtr<SVGRenderStyle> svg1 = SVGRenderStyle::create();
        RefPtr<SVGRenderStyle> svg2 = SVGRenderStyle::create();
        svg1->setStrokePaint(SVGRenderStyle::initialStrokePaintType(), SVGRenderStyle::initialStrokePaintColor(), SVGRenderStyle::initialStrokePaintUri(), true, false);
        svg2->setStrokePaint(SVGRenderStyle::initialStrokePaintType(), SVGRenderStyle::initialStrokePaintColor(), SVGRenderStyle::initialStrokePaintUri(), true, false);
        EXPECT_FALSE(svg1->diff(svg2.get()).hasDifference());
    }
    {
        RefPtr<SVGRenderStyle> svg1 = SVGRenderStyle::create();
        RefPtr<SVGRenderStyle> svg2 = SVGRenderStyle::create();
        svg1->setStrokePaint(SVGRenderStyle::initialStrokePaintType(), SVGRenderStyle::initialStrokePaintColor(), SVGRenderStyle::initialStrokePaintUri(), false, true);
        svg2->setStrokePaint(SVGRenderStyle::initialStrokePaintType(), SVGRenderStyle::initialStrokePaintColor(), SVGRenderStyle::initialStrokePaintUri(), false, true);
        EXPECT_FALSE(svg1->diff(svg2.get()).hasDifference());
    }
}

TEST(SVGRenderStyleTest, MiscStyleShouldCompareValue)
{
    TEST_STYLE_VALUE_NO_DIFF(Color, FloodColor);
    TEST_STYLE_VALUE_NO_DIFF(float, FloodOpacity);
    TEST_STYLE_VALUE_NO_DIFF(Color, LightingColor);
    TEST_STYLE_REFPTR_VALUE_NO_DIFF(SVGLength, BaselineShiftValue);
}

}
