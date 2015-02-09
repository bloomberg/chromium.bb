// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/style/LayoutStyle.h"

#include "core/layout/ClipPathOperation.h"
#include "core/layout/style/ShapeValue.h"

#include <gtest/gtest.h>

using namespace blink;

namespace {

TEST(LayoutStyleTest, ShapeOutsideBoxEqual)
{
    RefPtr<ShapeValue> shape1 = ShapeValue::createBoxShapeValue(ContentBox);
    RefPtr<ShapeValue> shape2 = ShapeValue::createBoxShapeValue(ContentBox);
    RefPtr<LayoutStyle> style1 = LayoutStyle::create();
    RefPtr<LayoutStyle> style2 = LayoutStyle::create();
    style1->setShapeOutside(shape1);
    style2->setShapeOutside(shape2);
    ASSERT_EQ(*style1, *style2);
}

TEST(LayoutStyleTest, ShapeOutsideCircleEqual)
{
    RefPtr<BasicShapeCircle> circle1 = BasicShapeCircle::create();
    RefPtr<BasicShapeCircle> circle2 = BasicShapeCircle::create();
    RefPtr<ShapeValue> shape1 = ShapeValue::createShapeValue(circle1, ContentBox);
    RefPtr<ShapeValue> shape2 = ShapeValue::createShapeValue(circle2, ContentBox);
    RefPtr<LayoutStyle> style1 = LayoutStyle::create();
    RefPtr<LayoutStyle> style2 = LayoutStyle::create();
    style1->setShapeOutside(shape1);
    style2->setShapeOutside(shape2);
    ASSERT_EQ(*style1, *style2);
}

TEST(LayoutStyleTest, ClipPathEqual)
{
    RefPtr<BasicShapeCircle> shape = BasicShapeCircle::create();
    RefPtr<ShapeClipPathOperation> path1 = ShapeClipPathOperation::create(shape);
    RefPtr<ShapeClipPathOperation> path2 = ShapeClipPathOperation::create(shape);
    RefPtr<LayoutStyle> style1 = LayoutStyle::create();
    RefPtr<LayoutStyle> style2 = LayoutStyle::create();
    style1->setClipPath(path1);
    style2->setClipPath(path2);
    ASSERT_EQ(*style1, *style2);
}

}
