// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderBlock.h"

#include "core/rendering/RenderBlockFlow.h"
#include "core/rendering/RenderingTestHelper.h"
#include <gtest/gtest.h>

namespace blink {

class RenderBlockTest : public RenderingTest {
};

TEST_F(RenderBlockTest, RenderNameCalledWithNullStyle)
{
    RenderObject* obj = RenderBlockFlow::createAnonymous(&document());
    EXPECT_FALSE(obj->style());
    EXPECT_STREQ("RenderBlock (generated)", obj->renderName());
    obj->destroy();
}

}
