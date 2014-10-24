// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/resolver/FontBuilder.h"
#include "core/testing/DummyPageHolder.h"

#include <gtest/gtest.h>

namespace blink {

class FontBuilderTest : public ::testing::Test {
protected:
    RenderStyle* getStyle(const FontBuilder& builder)
    {
        return builder.m_style;
    }
};

TEST_F(FontBuilderTest, StylePointerInitialisation)
{
    OwnPtr<DummyPageHolder> dummy = DummyPageHolder::create(IntSize(800, 600));
    FontBuilder builder(dummy->document());
    EXPECT_EQ(0, getStyle(builder));
}

} // namespace blink
