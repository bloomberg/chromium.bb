// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderInline.h"

#include "core/rendering/RenderingTestHelper.h"
#include <gtest/gtest.h>

namespace blink {

class RenderInlineTest : public RenderingTest {
};

TEST_F(RenderInlineTest, RenderNameCalledWithNullStyle)
{
    RenderObject* obj = RenderInline::createAnonymous(&document());
    EXPECT_FALSE(obj->style());
    EXPECT_STREQ("RenderInline (generated)", obj->renderName());
    obj->destroy();
}

}
