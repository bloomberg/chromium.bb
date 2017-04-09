// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutVideo.h"

#include "core/layout/LayoutTestHelper.h"

namespace blink {

using LayoutMediaTest = RenderingTest;

TEST_F(LayoutMediaTest, DisallowInlineChild) {
  SetBodyInnerHTML(
      "<style>"
      "  ::-webkit-media-controls { display: inline; }"
      "</style>"
      "<video id='video'></video>");

  EXPECT_FALSE(GetLayoutObjectByElementId("video")->SlowFirstChild());
}

TEST_F(LayoutMediaTest, DisallowBlockChild) {
  SetBodyInnerHTML(
      "<style>"
      "  ::-webkit-media-controls { display: block; }"
      "</style>"
      "<video id='video'></video>");

  EXPECT_FALSE(GetLayoutObjectByElementId("video")->SlowFirstChild());
}

TEST_F(LayoutMediaTest, DisallowOutOfFlowPositionedChild) {
  SetBodyInnerHTML(
      "<style>"
      "  ::-webkit-media-controls { position: absolute; }"
      "</style>"
      "<video id='video'></video>");

  EXPECT_FALSE(GetLayoutObjectByElementId("video")->SlowFirstChild());
}

TEST_F(LayoutMediaTest, DisallowFloatingChild) {
  SetBodyInnerHTML(
      "<style>"
      "  ::-webkit-media-controls { float: left; }"
      "</style>"
      "<video id='video'></video>");

  EXPECT_FALSE(GetLayoutObjectByElementId("video")->SlowFirstChild());
}

}  // namespace blink
