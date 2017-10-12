// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutTestHelper.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class LayoutCountTest : public RenderingTest {};

TEST_F(LayoutCountTest, SimpleBlockLayoutIsOnePass) {
  ScopedTrackLayoutPassesPerBlockForTest track_layout_passes_per_block(true);
  ScopedRootLayerScrollingForTest root_layer_scrolling(true);
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      " <div id='block' style='height:1000px'>Item</div>");

  LayoutBlockFlow* block = ToLayoutBlockFlow(
      GetDocument().getElementById("block")->GetLayoutObject());
  ASSERT_EQ(block->GetLayoutPassCountForTesting(), 1);
}

}  // namespace blink
