// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ClipPathClipper.h"

#include <gtest/gtest.h>
#include "core/testing/CoreUnitTestHelper.h"

namespace blink {
namespace {

using ClipPathClipperTest = RenderingTest;

TEST_F(ClipPathClipperTest, ClipPathBoundingBoxClamped) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="e" style="width:1000px; height:1000px; will-change:transform;
                       clip-path:circle(1000000000%);">
    </div>
  )HTML");
  auto& object = *GetLayoutObjectByElementId("e");
  Optional<FloatRect> bounding_box =
      ClipPathClipper::LocalClipPathBoundingBox(object);
  ASSERT_TRUE(bounding_box.has_value());
  EXPECT_EQ(LayoutRect::InfiniteIntRect(), *bounding_box);
}

}  // unnamed namespace
}  // namespace blink
