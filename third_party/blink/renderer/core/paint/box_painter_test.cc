// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"

using testing::ElementsAre;

namespace blink {

using BoxPainterTest = PaintControllerPaintTest;

INSTANTIATE_PAINT_TEST_SUITE_P(BoxPainterTest);

TEST_P(BoxPainterTest, DontPaintEmptyDecorationBackground) {
  SetBodyInnerHTML(R"HTML(
    <div id="div1" style="width: 100px; height: 100px; background: green">
    </div>
    <div id="div2" style="width: 100px; height: 100px; outline: 2px solid blue">
    </div>
  )HTML");

  auto* div1 = GetLayoutObjectByElementId("div1");
  auto* div2 = GetLayoutObjectByElementId("div2");
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                          IsSameId(div1, kBackgroundType),
                          IsSameId(div2, DisplayItem::PaintPhaseToDrawingType(
                                             PaintPhase::kSelfOutlineOnly))));
}

}  // namespace blink
