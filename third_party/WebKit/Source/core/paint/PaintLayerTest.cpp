// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayer.h"

#include "core/layout/LayoutReplica.h"
#include "core/layout/LayoutTestHelper.h"

namespace blink {

class PaintLayerTest : public RenderingTest {
};

TEST_F(PaintLayerTest, ReflectionLeak)
{
    setBodyInnerHTML("<div id='div' style='-webkit-box-reflect: above'>test</div>");
    HTMLElement* div = toHTMLElement(document().getElementById(AtomicString("div")));
    PaintLayerReflectionInfo* reflectionInfo = div->layoutBoxModelObject()->layer()->reflectionInfo();
    ASSERT_TRUE(reflectionInfo);
    LayoutReplica* replica = reflectionInfo->reflection();
    ASSERT_TRUE(replica);

    div->setAttribute(HTMLNames::styleAttr, "-webkit-box-reflect: below");
    document().view()->updateAllLifecyclePhases();

    PaintLayerReflectionInfo* newReflectionInfo = div->layoutBoxModelObject()->layer()->reflectionInfo();
    ASSERT_TRUE(newReflectionInfo);
    EXPECT_NE(reflectionInfo, newReflectionInfo);
    LayoutReplica* newReplica = newReflectionInfo->reflection();
    ASSERT_TRUE(newReplica);
    EXPECT_NE(replica, newReplica);

    // LeakDetector should report no leaks of LayoutReplica or PaintLayer.
}

} // namespace blink
