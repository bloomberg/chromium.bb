// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/FrameView.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PaintLayerScrollableAreaTest : public RenderingTest {
public:
    PaintLayerScrollableAreaTest()
        : RenderingTest(SingleChildFrameLoaderClient::create())
    { }

    bool shouldPaintBackgroundOntoScrollingContentsLayer(const char* elementId)
    {
        PaintLayer* paintLayer = toLayoutBlock(getLayoutObjectByElementId(elementId))->layer();
        return paintLayer->shouldPaintBackgroundOntoScrollingContentsLayer();
    }

private:
    void SetUp() override
    {
        RenderingTest::SetUp();
        enableCompositing();
    }

    void TearDown() override
    {
        RenderingTest::TearDown();
    }
};

}
