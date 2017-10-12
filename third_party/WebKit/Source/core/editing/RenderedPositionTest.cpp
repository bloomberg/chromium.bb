// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/RenderedPosition.h"

#include "build/build_config.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class RenderedPositionTest : public EditingTestBase {};

#if defined(OS_ANDROID)
#define MAYBE_IsVisible DISABLED_IsVisible
#else
#define MAYBE_IsVisible IsVisible
#endif
TEST_F(RenderedPositionTest, MAYBE_IsVisible) {
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

TEST_F(RenderedPositionTest, GetSamplePointForVisibility) {
  LayoutPoint top(-1, 10);
  LayoutPoint bottom(20, 10);
  EXPECT_EQ(LayoutPoint(19, 10),
            RenderedPosition::GetSamplePointForVisibility(top, bottom));
}

#undef MAYBE_IsVisible
}  // namespace blink
