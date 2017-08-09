// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/RenderedPosition.h"

#include "core/css/CSSStyleDeclaration.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/VisibleUnits.h"

namespace blink {

class RenderedPositionTest : public EditingTestBase {};

TEST_F(RenderedPositionTest, IsVisible) {
  SetBodyContent(
      "<input id=target width=100 value='test test test test test tes tes test'"
      " style='will-change: transform; font-size:40pt;'/>");

  Element* target = GetDocument().getElementById("target");
  Position visible_position(
      target->GetLayoutObject()->SlowFirstChild()->SlowFirstChild()->GetNode(),
      0);
  RenderedPosition rendered_visible_position(
      CreateVisiblePosition(visible_position));
  EXPECT_TRUE(rendered_visible_position.IsVisible(true));

  Position hidden_position(
      target->GetLayoutObject()->SlowFirstChild()->SlowFirstChild()->GetNode(),
      32);
  RenderedPosition rendered_hidden_position(
      CreateVisiblePosition(hidden_position));
  EXPECT_FALSE(rendered_hidden_position.IsVisible(true));
}

}  // namespace blink
