// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/dom/FrameRequestCallback.h"
#include "core/geometry/DOMRect.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/PaintLayer.h"
#include "core/testing/sim/SimCompositor.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DocumentLoadingRenderingTest : public SimTest {};

TEST_F(DocumentLoadingRenderingTest,
       ShouldResumeCommitsAfterBodyParsedWithoutSheets) {
  SimRequest main_resource("https://example.com/test.html", "text/html");

  LoadURL("https://example.com/test.html");

  main_resource.Start();

  // Still in the head, should not resume commits.
  main_resource.Write("<!DOCTYPE html>");
  EXPECT_TRUE(Compositor().DeferCommits());
  main_resource.Write("<title>Test</title><style>div { color red; }</style>");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Implicitly inserts the body. Since there's no loading stylesheets we
  // should resume commits.
  main_resource.Write("<p>Hello World</p>");
  EXPECT_FALSE(Compositor().DeferCommits());

  // Finish the load, should stay resumed.
  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferCommits());
}

TEST_F(DocumentLoadingRenderingTest,
       ShouldResumeCommitsAfterBodyIfSheetsLoaded) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest css_resource("https://example.com/test.css", "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Start();

  // Still in the head, should not resume commits.
  main_resource.Write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Sheet is streaming in, but not ready yet.
  css_resource.Start();
  css_resource.Write("a { color: red; }");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Sheet finished, but no body yet, so don't resume.
  css_resource.Finish();
  EXPECT_TRUE(Compositor().DeferCommits());

  // Body inserted and sheet is loaded so resume commits.
  main_resource.Write("<body>");
  EXPECT_FALSE(Compositor().DeferCommits());

  // Finish the load, should stay resumed.
  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterSheetsLoaded) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest css_resource("https://example.com/test.css", "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Start();

  // Still in the head, should not resume commits.
  main_resource.Write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Sheet is streaming in, but not ready yet.
  css_resource.Start();
  css_resource.Write("a { color: red; }");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Body inserted, but sheet is still loading so don't resume.
  main_resource.Write("<body>");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Sheet finished and there's a body so resume.
  css_resource.Finish();
  EXPECT_FALSE(Compositor().DeferCommits());

  // Finish the load, should stay resumed.
  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferCommits());
}

TEST_F(DocumentLoadingRenderingTest,
       ShouldResumeCommitsAfterDocumentElementWithNoSheets) {
  SimRequest main_resource("https://example.com/test.svg", "image/svg+xml");
  SimRequest css_resource("https://example.com/test.css", "text/css");

  LoadURL("https://example.com/test.svg");

  main_resource.Start();

  // Sheet loading and no documentElement, so don't resume.
  main_resource.Write("<?xml-stylesheet type='text/css' href='test.css'?>");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Sheet finishes loading, but no documentElement yet so don't resume.
  css_resource.Complete("a { color: red; }");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Root inserted so resume.
  main_resource.Write("<svg xmlns='http://www.w3.org/2000/svg'></svg>");
  EXPECT_FALSE(Compositor().DeferCommits());

  // Finish the load, should stay resumed.
  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterSheetsLoadForXml) {
  SimRequest main_resource("https://example.com/test.svg", "image/svg+xml");
  SimRequest css_resource("https://example.com/test.css", "text/css");

  LoadURL("https://example.com/test.svg");

  main_resource.Start();

  // Not done parsing.
  main_resource.Write("<?xml-stylesheet type='text/css' href='test.css'?>");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Sheet is streaming in, but not ready yet.
  css_resource.Start();
  css_resource.Write("a { color: red; }");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Root inserted, but sheet is still loading so don't resume.
  main_resource.Write("<svg xmlns='http://www.w3.org/2000/svg'></svg>");
  EXPECT_TRUE(Compositor().DeferCommits());

  // Finish the load, but sheets still loading so don't resume.
  main_resource.Finish();
  EXPECT_TRUE(Compositor().DeferCommits());

  // Sheet finished, so resume commits.
  css_resource.Finish();
  EXPECT_FALSE(Compositor().DeferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterFinishParsingXml) {
  SimRequest main_resource("https://example.com/test.svg", "image/svg+xml");

  LoadURL("https://example.com/test.svg");

  main_resource.Start();

  // Finish parsing, no sheets loading so resume.
  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeImmediatelyForImageDocuments) {
  SimRequest main_resource("https://example.com/test.png", "image/png");

  LoadURL("https://example.com/test.png");

  main_resource.Start();
  EXPECT_TRUE(Compositor().DeferCommits());

  // Not really a valid image but enough for the test. ImageDocuments should
  // resume painting as soon as the first bytes arrive.
  main_resource.Write("image data");
  EXPECT_FALSE(Compositor().DeferCommits());

  main_resource.Finish();
  EXPECT_FALSE(Compositor().DeferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldScheduleFrameAfterSheetsLoaded) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest first_css_resource("https://example.com/first.css", "text/css");
  SimRequest second_css_resource("https://example.com/second.css", "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Start();

  // Load a stylesheet.
  main_resource.Write(
      "<!DOCTYPE html><link id=link rel=stylesheet href=first.css>");
  EXPECT_TRUE(Compositor().DeferCommits());

  first_css_resource.Start();
  first_css_resource.Write("body { color: red; }");
  main_resource.Write("<body>");
  first_css_resource.Finish();

  // Sheet finished and there's a body so resume.
  EXPECT_FALSE(Compositor().DeferCommits());

  main_resource.Finish();
  Compositor().BeginFrame();

  // Replace the stylesheet by changing href.
  auto* element = GetDocument().getElementById("link");
  EXPECT_NE(nullptr, element);
  element->setAttribute(HTMLNames::hrefAttr, "second.css");
  EXPECT_FALSE(Compositor().NeedsBeginFrame());

  second_css_resource.Complete("body { color: red; }");
  EXPECT_TRUE(Compositor().NeedsBeginFrame());
}

TEST_F(DocumentLoadingRenderingTest,
       ShouldNotPaintIframeContentWithPendingSheets) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");
  SimRequest css_resource("https://example.com/test.css", "text/css");

  LoadURL("https://example.com/test.html");

  WebView().Resize(WebSize(800, 600));

  main_resource.Complete(
      "<!DOCTYPE html>"
      "<body style='background: red'>"
      "<iframe id=frame src=frame.html style='border: none'></iframe>"
      "<p style='transform: translateZ(0)'>Hello World</p>");

  // Main page is ready to begin painting as there's no pending sheets.
  // The frame is not yet loaded, so we only paint the top level page.
  auto frame1 = Compositor().BeginFrame();
  EXPECT_TRUE(frame1.Contains(SimCanvas::kText));

  frame_resource.Complete(
      "<!DOCTYPE html>"
      "<style>html { background: pink }</style>"
      "<link rel=stylesheet href=test.css>"
      "<p style='background: yellow;'>Hello World</p>"
      "<div style='transform: translateZ(0); background: green;'>"
      "    <p style='background: blue;'>Hello Layer</p>"
      "    <div style='position: relative; background: red;'>Hello World</div>"
      "</div>");

  // Trigger a layout with pending sheets. For example a page could trigger
  // this by doing offsetTop in a setTimeout, or by a parent frame executing
  // script that touched offsetTop in the child frame.
  auto* child_frame =
      toHTMLIFrameElement(GetDocument().getElementById("frame"));
  child_frame->contentDocument()
      ->UpdateStyleAndLayoutIgnorePendingStylesheets();

  auto frame2 = Compositor().BeginFrame();

  // The child frame still has pending sheets, and the parent frame has no
  // invalid paint so we shouldn't draw any text.
  EXPECT_FALSE(frame2.Contains(SimCanvas::kText));

  // 1 for the main frame background (red).
  // TODO(esprehn): If we were super smart we'd notice that the nested iframe is
  // actually composited and not repaint the main frame, but that likely
  // requires doing compositing and paint invalidation bottom up.
  EXPECT_EQ(1, frame2.DrawCount());
  EXPECT_TRUE(frame2.Contains(SimCanvas::kRect, "red"));

  // Finish loading the sheets in the child frame. After it should issue a
  // paint invalidation for every layer when the frame becomes unthrottled.
  css_resource.Complete();

  // First frame where all frames are loaded, should paint the text in the
  // child frame.
  auto frame3 = Compositor().BeginFrame();
  EXPECT_TRUE(frame3.Contains(SimCanvas::kText));
}

namespace {

class CheckRafCallback final : public FrameRequestCallback {
 public:
  void handleEvent(double high_res_time_ms) override { was_called_ = true; }
  bool WasCalled() const { return was_called_; }

 private:
  bool was_called_ = false;
};

}  // namespace

TEST_F(DocumentLoadingRenderingTest,
       ShouldThrottleIframeLifecycleUntilPendingSheetsLoaded) {
  SimRequest main_resource("https://example.com/main.html", "text/html");
  SimRequest frame_resource("https://example.com/frame.html", "text/html");
  SimRequest css_resource("https://example.com/frame.css", "text/css");

  LoadURL("https://example.com/main.html");

  WebView().Resize(WebSize(800, 600));

  main_resource.Complete(
      "<!DOCTYPE html>"
      "<body style='background: red'>"
      "<iframe id=frame src=frame.html></iframe>");

  frame_resource.Complete(
      "<!DOCTYPE html>"
      "<link rel=stylesheet href=frame.css>"
      "<body style='background: blue'>");

  auto* child_frame =
      toHTMLIFrameElement(GetDocument().getElementById("frame"));

  // Frame while the child frame still has pending sheets.
  auto* frame1_callback = new CheckRafCallback();
  child_frame->contentDocument()->RequestAnimationFrame(frame1_callback);
  auto frame1 = Compositor().BeginFrame();
  EXPECT_FALSE(frame1_callback->WasCalled());
  EXPECT_TRUE(frame1.Contains(SimCanvas::kRect, "red"));
  EXPECT_FALSE(frame1.Contains(SimCanvas::kRect, "blue"));

  // Finish loading the sheets in the child frame. Should enable lifecycle
  // updates and raf callbacks.
  css_resource.Complete();

  // Frame with all lifecycle updates enabled.
  auto* frame2_callback = new CheckRafCallback();
  child_frame->contentDocument()->RequestAnimationFrame(frame2_callback);
  auto frame2 = Compositor().BeginFrame();
  EXPECT_TRUE(frame1_callback->WasCalled());
  EXPECT_TRUE(frame2_callback->WasCalled());
  EXPECT_TRUE(frame2.Contains(SimCanvas::kRect, "red"));
  EXPECT_TRUE(frame2.Contains(SimCanvas::kRect, "blue"));
}

TEST_F(DocumentLoadingRenderingTest,
       ShouldContinuePaintingWhenSheetsStartedAfterBody) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest css_head_resource("https://example.com/testHead.css", "text/css");
  SimRequest css_body_resource("https://example.com/testBody.css", "text/css");

  LoadURL("https://example.com/test.html");

  main_resource.Start();

  // Still in the head, should not paint.
  main_resource.Write("<!DOCTYPE html><link rel=stylesheet href=testHead.css>");
  EXPECT_FALSE(GetDocument().IsRenderingReady());

  // Sheet is streaming in, but not ready yet.
  css_head_resource.Start();
  css_head_resource.Write("a { color: red; }");
  EXPECT_FALSE(GetDocument().IsRenderingReady());

  // Body inserted but sheet is still pending so don't paint.
  main_resource.Write("<body>");
  EXPECT_FALSE(GetDocument().IsRenderingReady());

  // Sheet finished and body inserted, ok to paint.
  css_head_resource.Finish();
  EXPECT_TRUE(GetDocument().IsRenderingReady());

  // In the body, should not stop painting.
  main_resource.Write("<link rel=stylesheet href=testBody.css>");
  EXPECT_TRUE(GetDocument().IsRenderingReady());

  // Finish loading the CSS resource (no change to painting).
  css_body_resource.Complete("a { color: red; }");
  EXPECT_TRUE(GetDocument().IsRenderingReady());

  // Finish the load, painting should stay enabled.
  main_resource.Finish();
  EXPECT_TRUE(GetDocument().IsRenderingReady());
}

TEST_F(DocumentLoadingRenderingTest,
       returnBoundingClientRectCorrectlyWhileLoadingImport) {
  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimRequest import_resource("https://example.com/import.css", "text/css");

  LoadURL("https://example.com/test.html");

  WebView().Resize(WebSize(800, 600));

  main_resource.Start();

  main_resource.Write(
      "<html><body>"
      "  <div id='test' style='font-size: 16px'>test</div>"
      "  <script>"
      "    var link = document.createElement('link');"
      "    link.rel = 'import';"
      "    link.href = 'import.css';"
      "    document.head.appendChild(link);"
      "  </script>");
  import_resource.Start();

  // Import loader isn't finish, shoudn't paint.
  EXPECT_FALSE(GetDocument().IsRenderingReady());

  // If ignoringPendingStylesheets==true, element should get non-empty rect.
  Element* element = GetDocument().getElementById("test");
  DOMRect* rect = element->getBoundingClientRect();
  EXPECT_TRUE(rect->width() > 0.f);
  EXPECT_TRUE(rect->height() > 0.f);

  // After reset ignoringPendingStylesheets, we should block rendering again.
  EXPECT_FALSE(GetDocument().IsRenderingReady());

  import_resource.Write("div { color: red; }");
  import_resource.Finish();
  main_resource.Finish();
}

}  // namespace blink
