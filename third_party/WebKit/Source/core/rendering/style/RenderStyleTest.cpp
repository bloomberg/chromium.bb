// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/style/RenderStyle.h"

#include "core/rendering/ClipPathOperation.h"
#include "core/rendering/style/ShapeValue.h"

#include <gtest/gtest.h>

using namespace blink;

namespace {

TEST(RenderStyleTest, ShapeOutsideBoxEqual)
{
    RefPtr<ShapeValue> shape1 = ShapeValue::createBoxShapeValue(ContentBox);
    RefPtr<ShapeValue> shape2 = ShapeValue::createBoxShapeValue(ContentBox);
    RefPtr<RenderStyle> style1 = RenderStyle::create();
    RefPtr<RenderStyle> style2 = RenderStyle::create();
    style1->setShapeOutside(shape1);
    style2->setShapeOutside(shape2);
    ASSERT_EQ(*style1, *style2);
}

TEST(RenderStyleTest, ShapeOutsideCircleEqual)
{
    RefPtr<BasicShapeCircle> circle1 = BasicShapeCircle::create();
    RefPtr<BasicShapeCircle> circle2 = BasicShapeCircle::create();
    RefPtr<ShapeValue> shape1 = ShapeValue::createShapeValue(circle1, ContentBox);
    RefPtr<ShapeValue> shape2 = ShapeValue::createShapeValue(circle2, ContentBox);
    RefPtr<RenderStyle> style1 = RenderStyle::create();
    RefPtr<RenderStyle> style2 = RenderStyle::create();
    style1->setShapeOutside(shape1);
    style2->setShapeOutside(shape2);
    ASSERT_EQ(*style1, *style2);
}

TEST(RenderStyleTest, ClipPathEqual)
{
    RefPtr<BasicShapeCircle> shape = BasicShapeCircle::create();
    RefPtr<ShapeClipPathOperation> path1 = ShapeClipPathOperation::create(shape);
    RefPtr<ShapeClipPathOperation> path2 = ShapeClipPathOperation::create(shape);
    RefPtr<RenderStyle> style1 = RenderStyle::create();
    RefPtr<RenderStyle> style2 = RenderStyle::create();
    style1->setClipPath(path1);
    style2->setClipPath(path2);
    ASSERT_EQ(*style1, *style2);
}

}
