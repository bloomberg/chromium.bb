// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScrollIntoViewOptionsOrBoolean.h"
#include "core/frame/ScrollIntoViewOptions.h"
#include "core/frame/ScrollToOptions.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/testing/sim/SimCompositor.h"
#include "core/testing/sim/SimDisplayItemList.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "public/web/WebFindOptions.h"
#include "public/web/WebScriptSource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class SmoothScrollTest : public SimTest {};

TEST_F(SmoothScrollTest, InstantScroll) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='space' style='height: 1000px'></div>"
      "<div id='content' style='height: 1000px'></div>");

  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions options;
  options.setBlock("start");
  arg.setScrollIntoViewOptions(options);
  content->scrollIntoView(arg);

  ASSERT_EQ(Window().scrollY(), content->OffsetTop());
}

TEST_F(SmoothScrollTest, SmoothScroll) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='space' style='height: 1000px'></div>"
      "<div id='content' style='height: 1000px'></div>");

  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions options;
  options.setBlock("start");
  options.setBehavior("smooth");
  arg.setScrollIntoViewOptions(options);
  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);

  content->scrollIntoView(arg);
  // Scrolling the container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(Window().scrollY(), 299);

  // Finish scrolling the container
  Compositor().BeginFrame(1);
  ASSERT_EQ(Window().scrollY(), content->OffsetTop());
}

TEST_F(SmoothScrollTest, NestedContainer) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='space' style='height: 1000px'></div>"
      "<div id='container' style='height: 600px; overflow: scroll'>"
      "  <div id='space1' style='height: 1000px'></div>"
      "  <div id='content' style='height: 1000px'></div>"
      "</div>");

  Element* container = GetDocument().getElementById("container");
  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions options;
  options.setBlock("start");
  options.setBehavior("smooth");
  arg.setScrollIntoViewOptions(options);
  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  ASSERT_EQ(container->scrollTop(), 0);

  content->scrollIntoView(arg);
  // Scrolling the outer container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(Window().scrollY(), 299);
  ASSERT_EQ(container->scrollTop(), 0);

  // Finish scrolling the outer container
  Compositor().BeginFrame(1);
  ASSERT_EQ(Window().scrollY(), container->OffsetTop());
  ASSERT_EQ(container->scrollTop(), 0);

  // Scrolling the inner container
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(container->scrollTop(), 299);

  // Finish scrolling the inner container
  Compositor().BeginFrame(1);
  ASSERT_EQ(container->scrollTop(),
            content->OffsetTop() - container->OffsetTop());
}

TEST_F(SmoothScrollTest, NewScrollIntoViewAbortsCurrentAnimation) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='container2' style='height: 1000px; overflow: scroll'>"
      "  <div id='space2' style='height: 1200px'></div>"
      "  <div id='content2' style='height: 1000px'></div>"
      "</div>"
      "<div id='container1' style='height: 600px; overflow: scroll'>"
      "  <div id='space1' style='height: 1000px'></div>"
      "  <div id='content1' style='height: 1000px'></div>"
      "</div>");

  Element* container1 = GetDocument().getElementById("container1");
  Element* container2 = GetDocument().getElementById("container2");
  Element* content1 = GetDocument().getElementById("content1");
  Element* content2 = GetDocument().getElementById("content2");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions options;
  options.setBlock("start");
  options.setBehavior("smooth");
  arg.setScrollIntoViewOptions(options);

  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  ASSERT_EQ(container1->scrollTop(), 0);
  ASSERT_EQ(container2->scrollTop(), 0);

  content1->scrollIntoView(arg);
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(Window().scrollY(), 299);
  ASSERT_EQ(container1->scrollTop(), 0);

  content2->scrollIntoView(arg);
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(Window().scrollY(), 61);
  ASSERT_EQ(container1->scrollTop(), 0);  // container1 should not scroll.

  Compositor().BeginFrame(1);
  ASSERT_EQ(Window().scrollY(), container2->OffsetTop());
  ASSERT_EQ(container2->scrollTop(), 0);

  // Scrolling content2 in container2
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(container2->scrollTop(), 300);

  // Finish all the animation to make sure there is no another animation queued
  // on container1.
  while (Compositor().NeedsBeginFrame()) {
    Compositor().BeginFrame();
  }
  ASSERT_EQ(Window().scrollY(), container2->OffsetTop());
  ASSERT_EQ(container2->scrollTop(),
            content2->OffsetTop() - container2->OffsetTop());
  ASSERT_EQ(container1->scrollTop(), 0);
}

TEST_F(SmoothScrollTest, ScrollWindowAbortsCurrentAnimation) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='space' style='height: 1000px'></div>"
      "<div id='container' style='height: 600px; overflow: scroll'>"
      "  <div id='space1' style='height: 1000px'></div>"
      "  <div id='content' style='height: 1000px'></div>"
      "</div>");

  Element* container = GetDocument().getElementById("container");
  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions options;
  options.setBlock("start");
  options.setBehavior("smooth");
  arg.setScrollIntoViewOptions(options);
  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  ASSERT_EQ(container->scrollTop(), 0);

  content->scrollIntoView(arg);
  // Scrolling the outer container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(Window().scrollY(), 299);
  ASSERT_EQ(container->scrollTop(), 0);

  ScrollToOptions window_option;
  window_option.setLeft(0);
  window_option.setTop(0);
  window_option.setBehavior("smooth");
  Window().scrollTo(window_option);
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(Window().scrollY(), 58);

  Compositor().BeginFrame(1);
  ASSERT_EQ(Window().scrollY(), 0);
  ASSERT_EQ(container->scrollTop(), 0);
}

TEST_F(SmoothScrollTest, BlockAndInlineSettings) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='container' style='height: 2500px; width: 2500px;'>"
      "<div id='content' style='height: 500px; width: 500px;"
      "margin-left: 1000px; margin-right: 1000px; margin-top: 1000px;"
      "margin-bottom: 1000px'></div></div>");

  int content_height = 500;
  int content_width = 500;
  int window_height = 600;
  int window_width = 800;

  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg1, arg2, arg3, arg4;
  ScrollIntoViewOptions options;
  ASSERT_EQ(Window().scrollY(), 0);

  options.setBlock("nearest");
  options.setInlinePosition("nearest");
  arg1.setScrollIntoViewOptions(options);
  content->scrollIntoView(arg1);
  ASSERT_EQ(Window().scrollX(),
            content->OffsetLeft() + content_width - window_width);
  ASSERT_EQ(Window().scrollY(),
            content->OffsetTop() + content_height - window_height);

  options.setBlock("start");
  options.setInlinePosition("start");
  arg2.setScrollIntoViewOptions(options);
  content->scrollIntoView(arg2);
  ASSERT_EQ(Window().scrollX(), content->OffsetLeft());
  ASSERT_EQ(Window().scrollY(), content->OffsetTop());

  options.setBlock("center");
  options.setInlinePosition("center");
  arg3.setScrollIntoViewOptions(options);
  content->scrollIntoView(arg3);
  ASSERT_EQ(Window().scrollX(),
            content->OffsetLeft() + (content_width - window_width) / 2);
  ASSERT_EQ(Window().scrollY(),
            content->OffsetTop() + (content_height - window_height) / 2);

  options.setBlock("end");
  options.setInlinePosition("end");
  arg4.setScrollIntoViewOptions(options);
  content->scrollIntoView(arg4);
  ASSERT_EQ(Window().scrollX(),
            content->OffsetLeft() + content_width - window_width);
  ASSERT_EQ(Window().scrollY(),
            content->OffsetTop() + content_height - window_height);
}

TEST_F(SmoothScrollTest, SmoothAndInstantInChain) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='space' style='height: 1000px'></div>"
      "<div id='container' style='height: 600px; overflow: scroll;"
      "  scroll-behavior: smooth'>"
      "  <div id='space1' style='height: 1000px'></div>"
      "  <div id='inner_container' style='height: 1000px; overflow: scroll;'>"
      "    <div id='space2' style='height: 1000px'></div>"
      "    <div id='content' style='height: 1000px;'></div>"
      "  </div>"
      "</div>");

  Element* container = GetDocument().getElementById("container");
  Element* inner_container = GetDocument().getElementById("inner_container");
  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions options;
  options.setBlock("start");
  arg.setScrollIntoViewOptions(options);
  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);
  ASSERT_EQ(container->scrollTop(), 0);

  content->scrollIntoView(arg);
  // Instant scroll of the window should have finished.
  ASSERT_EQ(Window().scrollY(), container->OffsetTop());
  // Instant scroll of the inner container should not have started.
  ASSERT_EQ(container->scrollTop(), 0);
  // Smooth scroll should not have started.
  ASSERT_EQ(container->scrollTop(), 0);

  // Scrolling the container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(container->scrollTop(), 299);

  // Finish scrolling the container
  Compositor().BeginFrame(1);
  ASSERT_EQ(container->scrollTop(),
            inner_container->OffsetTop() - container->OffsetTop());
  // Instant scroll of the inner container should have finished.
  ASSERT_EQ(inner_container->scrollTop(),
            content->OffsetTop() - inner_container->OffsetTop());
}

TEST_F(SmoothScrollTest, SmoothScrollAnchor) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='container' style='height: 600px; overflow: scroll;"
      "  scroll-behavior: smooth'>"
      "  <div id='space' style='height: 1000px'></div>"
      "  <div style='height: 1000px'><a name='link' "
      "id='content'>hello</a></div>"
      "</div>");

  Element* content = GetDocument().getElementById("content");
  Element* container = GetDocument().getElementById("container");
  KURL url(KURL(), "https://test.html/#link");
  LocalFrameView* frame_view = GetDocument().View();
  Compositor().BeginFrame();
  ASSERT_EQ(container->scrollTop(), 0);

  frame_view->ProcessUrlFragment(url);
  // Scrolling the container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(container->scrollTop(), 299);

  // Finish scrolling the container
  Compositor().BeginFrame(1);
  ASSERT_EQ(container->scrollTop(),
            content->OffsetTop() - container->OffsetTop());
}

TEST_F(SmoothScrollTest, FindDoesNotScrollOverflowHidden) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(
      "<div id='container' style='height: 400px; overflow: hidden;'>"
      "  <div id='space' style='height: 500px'></div>"
      "  <div style='height: 500px'>hello</div>"
      "</div>");
  Element* container = GetDocument().getElementById("container");
  Compositor().BeginFrame();
  ASSERT_EQ(container->scrollTop(), 0);
  const int kFindIdentifier = 12345;
  WebFindOptions options;
  MainFrame().Find(kFindIdentifier, WebString::FromUTF8("hello"), options,
                   false);
  ASSERT_EQ(container->scrollTop(), 0);
}

TEST_F(SmoothScrollTest, ApplyRootElementScrollBehaviorToViewport) {
  v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
  WebView().Resize(WebSize(800, 600));
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete("<html style='scroll-behavior: smooth'>"
      "<div id='space' style='height: 1000px'></div>"
      "<div id='content' style='height: 1000px'></div></html>");

  Element* content = GetDocument().getElementById("content");
  ScrollIntoViewOptionsOrBoolean arg;
  ScrollIntoViewOptions options;
  options.setBlock("start");
  arg.setScrollIntoViewOptions(options);
  Compositor().BeginFrame();
  ASSERT_EQ(Window().scrollY(), 0);

  content->scrollIntoView(arg);
  // Scrolling the container
  Compositor().BeginFrame();  // update run_state_.
  Compositor().BeginFrame();  // Set start_time = now.
  Compositor().BeginFrame(0.2);
  ASSERT_EQ(Window().scrollY(), 299);

  // Finish scrolling the container
  Compositor().BeginFrame(1);
  ASSERT_EQ(Window().scrollY(), content->OffsetTop());
}

}  // namespace

}  // namespace blink
