// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/layout/LayoutTestHelper.h"

namespace blink {

static void enableScrollAnchoring(Settings& settings)
{
    settings.setScrollAnchoringEnabled(true);
}

class ScrollAnchorTest : public RenderingTest {
    FrameSettingOverrideFunction settingOverrider() const override { return enableScrollAnchoring; }
};

TEST_F(ScrollAnchorTest, Basic)
{
    ScrollAnchor scrollAnchor(document().view()->layoutViewportScrollableArea());
    scrollAnchor.clear();
    EXPECT_EQ(nullptr, scrollAnchor.anchorObject());
}
}
