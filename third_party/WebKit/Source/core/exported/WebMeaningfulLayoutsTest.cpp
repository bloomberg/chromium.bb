// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/testing/sim/SimCompositor.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class WebMeaningfulLayoutsTest : public SimTest {};

TEST_F(WebMeaningfulLayoutsTest, VisuallyNonEmptyTextCharacters) {
  SimRequest main_resource("https://example.com/index.html", "text/html");

  LoadURL("https://example.com/index.html");

  main_resource.Start();

  // Write 201 characters.
  const char* ten_characters = "0123456789";
  for (int i = 0; i < 20; ++i)
    main_resource.Write(ten_characters);
  main_resource.Write("!");

  main_resource.Finish();

  Compositor().BeginFrame();

  EXPECT_EQ(1, WebViewClient().VisuallyNonEmptyLayoutCount());
}

TEST_F(WebMeaningfulLayoutsTest, VisuallyNonEmptyTextCharactersEventually) {
  SimRequest main_resource("https://example.com/index.html", "text/html");

  LoadURL("https://example.com/index.html");

  main_resource.Start();

  // Write 200 characters.
  const char* ten_characters = "0123456789";
  for (int i = 0; i < 20; ++i)
    main_resource.Write(ten_characters);

  // Pump a frame mid-load.
  Compositor().BeginFrame();

  EXPECT_EQ(0, WebViewClient().VisuallyNonEmptyLayoutCount());

  // Write more than 200 characters.
  main_resource.Write("!");

  main_resource.Finish();

  // setting visually non-empty happens when the parsing finishes,
  // not as the character count goes over 200.
  Compositor().BeginFrame();

  EXPECT_EQ(1, WebViewClient().VisuallyNonEmptyLayoutCount());
}

// TODO(dglazkov): Write pixel-count and canvas-based VisuallyNonEmpty tests

TEST_F(WebMeaningfulLayoutsTest, VisuallyNonEmptyMissingPump) {
  SimRequest main_resource("https://example.com/index.html", "text/html");

  LoadURL("https://example.com/index.html");

  main_resource.Start();

  // Write <200 characters.
  main_resource.Write("less than 200 characters.");

  Compositor().BeginFrame();

  main_resource.Finish();

  // Even though the layout state is clean ...
  EXPECT_TRUE(GetDocument().Lifecycle().GetState() >=
              DocumentLifecycle::kLayoutClean);

  // We should still generate a request for another (possibly last) frame.
  EXPECT_TRUE(Compositor().NeedsBeginFrame());

  // ... which we (the scheduler) happily provide.
  Compositor().BeginFrame();

  // ... which correctly signals the VisuallyNonEmpty.
  EXPECT_EQ(1, WebViewClient().VisuallyNonEmptyLayoutCount());
}

TEST_F(WebMeaningfulLayoutsTest, FinishedParsing) {
  SimRequest main_resource("https://example.com/index.html", "text/html");

  LoadURL("https://example.com/index.html");

  main_resource.Complete("content");

  Compositor().BeginFrame();

  EXPECT_EQ(1, WebViewClient().FinishedParsingLayoutCount());
}

TEST_F(WebMeaningfulLayoutsTest, FinishedLoading) {
  SimRequest main_resource("https://example.com/index.html", "text/html");

  LoadURL("https://example.com/index.html");

  main_resource.Complete("content");

  Compositor().BeginFrame();

  EXPECT_EQ(1, WebViewClient().FinishedLoadingLayoutCount());
}

TEST_F(WebMeaningfulLayoutsTest, FinishedParsingThenLoading) {
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimRequest image_resource("https://example.com/cat.png", "image/png");

  LoadURL("https://example.com/index.html");

  main_resource.Complete("<img src=cat.png>");

  Compositor().BeginFrame();

  EXPECT_EQ(1, WebViewClient().FinishedParsingLayoutCount());
  EXPECT_EQ(0, WebViewClient().FinishedLoadingLayoutCount());

  image_resource.Complete("image data");

  // Pump the message loop to process the image loading task.
  testing::RunPendingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(1, WebViewClient().FinishedParsingLayoutCount());
  EXPECT_EQ(1, WebViewClient().FinishedLoadingLayoutCount());
}

TEST_F(WebMeaningfulLayoutsTest, WithIFrames) {
  SimRequest main_resource("https://example.com/index.html", "text/html");
  SimRequest iframe_resource("https://example.com/iframe.html", "text/html");

  LoadURL("https://example.com/index.html");

  main_resource.Complete("<iframe src=iframe.html></iframe>");

  Compositor().BeginFrame();

  EXPECT_EQ(1, WebViewClient().VisuallyNonEmptyLayoutCount());
  EXPECT_EQ(1, WebViewClient().FinishedParsingLayoutCount());
  EXPECT_EQ(0, WebViewClient().FinishedLoadingLayoutCount());

  iframe_resource.Complete("iframe data");

  // Pump the message loop to process the iframe loading task.
  testing::RunPendingTasks();

  Compositor().BeginFrame();

  EXPECT_EQ(1, WebViewClient().VisuallyNonEmptyLayoutCount());
  EXPECT_EQ(1, WebViewClient().FinishedParsingLayoutCount());
  EXPECT_EQ(1, WebViewClient().FinishedLoadingLayoutCount());
}

// NoOverflowInIncrementVisuallyNonEmptyPixelCount tests fail if the number of
// pixels is calculated in 32-bit integer, because 65536 * 65536 would become 0
// if it was calculated in 32-bit and thus it would be considered as empty.
TEST_F(WebMeaningfulLayoutsTest,
       NoOverflowInIncrementVisuallyNonEmptyPixelCount) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest svg_resource("https://example.com/test.svg", "image/svg+xml");

  LoadURL("https://example.com/test.html");

  main_resource.Start();
  main_resource.Write("<DOCTYPE html><body><img src=\"test.svg\">");
  // Run pending tasks to initiate the request to test.svg.
  testing::RunPendingTasks();
  EXPECT_EQ(0, WebViewClient().VisuallyNonEmptyLayoutCount());

  // We serve the SVG file and check visuallyNonEmptyLayoutCount() before
  // mainResource.finish() because finishing the main resource causes
  // |FrameView::m_isVisuallyNonEmpty| to be true and
  // visuallyNonEmptyLayoutCount() to be 1 irrespective of the SVG sizes.
  svg_resource.Start();
  svg_resource.Write(
      "<svg xmlns=\"http://www.w3.org/2000/svg\" height=\"65536\" "
      "width=\"65536\"></svg>");
  svg_resource.Finish();
  Compositor().BeginFrame();
  EXPECT_EQ(1, WebViewClient().VisuallyNonEmptyLayoutCount());

  main_resource.Finish();
}

}  // namespace blink
