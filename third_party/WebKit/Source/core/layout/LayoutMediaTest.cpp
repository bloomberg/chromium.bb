// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutVideo.h"

#include "core/layout/LayoutTestHelper.h"

namespace blink {

using LayoutMediaTest = RenderingTest;

TEST_F(LayoutMediaTest, DisallowInlineChild) {
  setBodyInnerHTML(
      "<style>"
      "  ::-webkit-media-controls { display: inline; }"
      "</style>"
      "<video id='video'></video>");

  EXPECT_FALSE(getLayoutObjectByElementId("video")->slowFirstChild());
}

TEST_F(LayoutMediaTest, DisallowBlockChild) {
  setBodyInnerHTML(
      "<style>"
      "  ::-webkit-media-controls { display: block; }"
      "</style>"
      "<video id='video'></video>");

  EXPECT_FALSE(getLayoutObjectByElementId("video")->slowFirstChild());
}

TEST_F(LayoutMediaTest, DisallowOutOfFlowPositionedChild) {
  setBodyInnerHTML(
      "<style>"
      "  ::-webkit-media-controls { position: absolute; }"
      "</style>"
      "<video id='video'></video>");

  EXPECT_FALSE(getLayoutObjectByElementId("video")->slowFirstChild());
}

TEST_F(LayoutMediaTest, DisallowFloatingChild) {
  setBodyInnerHTML(
      "<style>"
      "  ::-webkit-media-controls { float: left; }"
      "</style>"
      "<video id='video'></video>");

  EXPECT_FALSE(getLayoutObjectByElementId("video")->slowFirstChild());
}

}  // namespace blink
