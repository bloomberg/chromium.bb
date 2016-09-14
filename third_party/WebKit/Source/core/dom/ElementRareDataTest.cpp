// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementRareData.h"

#include "core/layout/LayoutInline.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ElementRareDataTest, ComputedStyleStorage)
{
    // Without LayoutObject
    ElementRareData* rareData = ElementRareData::create(nullptr);
    RefPtr<ComputedStyle> style = ComputedStyle::create();
    rareData->setComputedStyle(style);
    EXPECT_EQ(style.get(), rareData->computedStyle());

    // With LayoutObject
    LayoutObject* layoutObject = new LayoutInline(nullptr);
    rareData = ElementRareData::create(layoutObject);
    rareData->setComputedStyle(style);
    EXPECT_EQ(style.get(), rareData->computedStyle());

    rareData->setLayoutObject(nullptr);
    delete layoutObject;
}

} // namespace blink
