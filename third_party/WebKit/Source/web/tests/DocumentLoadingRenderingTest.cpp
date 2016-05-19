// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/paint/PaintLayer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

using namespace HTMLNames;

class DocumentLoadingRenderingTest : public SimTest {
};

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterBodyParsedWithoutSheets)
{
    SimRequest mainResource("https://example.com/test.html", "text/html");

    loadURL("https://example.com/test.html");

    mainResource.start();

    // Still in the head, should not resume commits.
    mainResource.write("<!DOCTYPE html>");
    EXPECT_TRUE(compositor().deferCommits());
    mainResource.write("<title>Test</title><style>div { color red; }</style>");
    EXPECT_TRUE(compositor().deferCommits());

    // Implicitly inserts the body. Since there's no loading stylesheets we
    // should resume commits.
    mainResource.write("<p>Hello World</p>");
    EXPECT_FALSE(compositor().deferCommits());

    // Finish the load, should stay resumed.
    mainResource.finish();
    EXPECT_FALSE(compositor().deferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterBodyIfSheetsLoaded)
{
    SimRequest mainResource("https://example.com/test.html", "text/html");
    SimRequest cssResource("https://example.com/test.css", "text/css");

    loadURL("https://example.com/test.html");

    mainResource.start();

    // Still in the head, should not resume commits.
    mainResource.write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
    EXPECT_TRUE(compositor().deferCommits());

    // Sheet is streaming in, but not ready yet.
    cssResource.start();
    cssResource.write("a { color: red; }");
    EXPECT_TRUE(compositor().deferCommits());

    // Sheet finished, but no body yet, so don't resume.
    cssResource.finish();
    EXPECT_TRUE(compositor().deferCommits());

    // Body inserted and sheet is loaded so resume commits.
    mainResource.write("<body>");
    EXPECT_FALSE(compositor().deferCommits());

    // Finish the load, should stay resumed.
    mainResource.finish();
    EXPECT_FALSE(compositor().deferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterSheetsLoaded)
{
    SimRequest mainResource("https://example.com/test.html", "text/html");
    SimRequest cssResource("https://example.com/test.css", "text/css");

    loadURL("https://example.com/test.html");

    mainResource.start();

    // Still in the head, should not resume commits.
    mainResource.write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
    EXPECT_TRUE(compositor().deferCommits());

    // Sheet is streaming in, but not ready yet.
    cssResource.start();
    cssResource.write("a { color: red; }");
    EXPECT_TRUE(compositor().deferCommits());

    // Body inserted, but sheet is still loading so don't resume.
    mainResource.write("<body>");
    EXPECT_TRUE(compositor().deferCommits());

    // Sheet finished and there's a body so resume.
    cssResource.finish();
    EXPECT_FALSE(compositor().deferCommits());

    // Finish the load, should stay resumed.
    mainResource.finish();
    EXPECT_FALSE(compositor().deferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterDocumentElementWithNoSheets)
{
    SimRequest mainResource("https://example.com/test.svg", "image/svg+xml");
    SimRequest cssResource("https://example.com/test.css", "text/css");

    loadURL("https://example.com/test.svg");

    mainResource.start();

    // Sheet loading and no documentElement, so don't resume.
    mainResource.write("<?xml-stylesheet type='text/css' href='test.css'?>");
    EXPECT_TRUE(compositor().deferCommits());

    // Sheet finishes loading, but no documentElement yet so don't resume.
    cssResource.complete("a { color: red; }");
    EXPECT_TRUE(compositor().deferCommits());

    // Root inserted so resume.
    mainResource.write("<svg xmlns='http://www.w3.org/2000/svg'></svg>");
    EXPECT_FALSE(compositor().deferCommits());

    // Finish the load, should stay resumed.
    mainResource.finish();
    EXPECT_FALSE(compositor().deferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterSheetsLoadForXml)
{
    SimRequest mainResource("https://example.com/test.svg", "image/svg+xml");
    SimRequest cssResource("https://example.com/test.css", "text/css");

    loadURL("https://example.com/test.svg");

    mainResource.start();

    // Not done parsing.
    mainResource.write("<?xml-stylesheet type='text/css' href='test.css'?>");
    EXPECT_TRUE(compositor().deferCommits());

    // Sheet is streaming in, but not ready yet.
    cssResource.start();
    cssResource.write("a { color: red; }");
    EXPECT_TRUE(compositor().deferCommits());

    // Root inserted, but sheet is still loading so don't resume.
    mainResource.write("<svg xmlns='http://www.w3.org/2000/svg'></svg>");
    EXPECT_TRUE(compositor().deferCommits());

    // Finish the load, but sheets still loading so don't resume.
    mainResource.finish();
    EXPECT_TRUE(compositor().deferCommits());

    // Sheet finished, so resume commits.
    cssResource.finish();
    EXPECT_FALSE(compositor().deferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterFinishParsingXml)
{
    SimRequest mainResource("https://example.com/test.svg", "image/svg+xml");

    loadURL("https://example.com/test.svg");

    mainResource.start();

    // Finish parsing, no sheets loading so resume.
    mainResource.finish();
    EXPECT_FALSE(compositor().deferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeImmediatelyForImageDocuments)
{
    SimRequest mainResource("https://example.com/test.png", "image/png");

    loadURL("https://example.com/test.png");

    mainResource.start();
    EXPECT_TRUE(compositor().deferCommits());

    // Not really a valid image but enough for the test. ImageDocuments should
    // resume painting as soon as the first bytes arrive.
    mainResource.write("image data");
    EXPECT_FALSE(compositor().deferCommits());

    mainResource.finish();
    EXPECT_FALSE(compositor().deferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldScheduleFrameAfterSheetsLoaded)
{
    SimRequest mainResource("https://example.com/test.html", "text/html");
    SimRequest firstCssResource("https://example.com/first.css", "text/css");
    SimRequest secondCssResource("https://example.com/second.css", "text/css");

    loadURL("https://example.com/test.html");

    mainResource.start();

    // Load a stylesheet.
    mainResource.write("<!DOCTYPE html><link id=link rel=stylesheet href=first.css>");
    EXPECT_TRUE(compositor().deferCommits());

    firstCssResource.start();
    firstCssResource.write("body { color: red; }");
    mainResource.write("<body>");
    firstCssResource.finish();

    // Sheet finished and there's a body so resume.
    EXPECT_FALSE(compositor().deferCommits());

    mainResource.finish();
    compositor().beginFrame();

    // Replace the stylesheet by changing href.
    auto* element = document().getElementById("link");
    EXPECT_NE(nullptr, element);
    element->setAttribute(hrefAttr, "second.css");
    EXPECT_FALSE(compositor().needsAnimate());

    secondCssResource.complete("body { color: red; }");
    EXPECT_TRUE(compositor().needsAnimate());
}

TEST_F(DocumentLoadingRenderingTest, ShouldNotPaintIframeContentWithPendingSheets)
{
    SimRequest mainResource("https://example.com/test.html", "text/html");
    SimRequest frameResource("https://example.com/frame.html", "text/html");
    SimRequest cssResource("https://example.com/test.css", "text/css");

    loadURL("https://example.com/test.html");

    webView().resize(WebSize(800, 600));

    mainResource.complete(
        "<!DOCTYPE html>"
        "<iframe id=frame src=frame.html style='border: none'></iframe>"
        "<p style='transform: translateZ(0)'>Hello World</p>"
    );

    // Main page is ready to begin painting as there's no pending sheets.
    // The frame is not yet loaded, so we only paint the top level page.
    auto frame1 = compositor().beginFrame();
    EXPECT_TRUE(frame1.contains(SimCanvas::Text));

    frameResource.complete(
        "<!DOCTYPE html>"
        "<style>html { background: pink }</style>"
        "<link rel=stylesheet href=test.css>"
        "<p style='background: yellow;'>Hello World</p>"
        "<div style='transform: translateZ(0); background: green;'>"
        "    <p style='background: blue;'>Hello Layer</p>"
        "    <div style='position: relative; background: red;'>Hello World</div>"
        "</div>"
    );

    // Trigger a layout with pending sheets. For example a page could trigger
    // this by doing offsetTop in a setTimeout, or by a parent frame executing
    // script that touched offsetTop in the child frame.
    auto* childFrame = toHTMLIFrameElement(document().getElementById("frame"));
    childFrame->contentDocument()->updateStyleAndLayoutIgnorePendingStylesheets();

    auto frame2 = compositor().beginFrame();

    // The child frame still has pending sheets, and the parent frame has no
    // invalid paint so we shouldn't draw any text.
    EXPECT_FALSE(frame2.contains(SimCanvas::Text));

    LayoutViewItem iframeLayoutViewItem = childFrame->contentDocument()->layoutViewItem();
    const DisplayItemList& displayItemList = iframeLayoutViewItem.layer()->graphicsLayerBacking()->getPaintController().getDisplayItemList();
    // Check that the DisplayItemList has no subsequene caching markers. These are not allowed in pending-style-sheets mode
    // since otherwise caching would be incorrect.
    ASSERT_EQ(2u, displayItemList.size());
    EXPECT_EQ(DisplayItem::DocumentBackground, displayItemList[0].getType());
    EXPECT_EQ(DisplayItem::BoxDecorationBackground, displayItemList[1].getType());

    // 1 for the main frame background (white),
    // 1 for the iframe background (pink)
    // 1 for the composited transform layer in the iframe (green).
    // TODO(esprehn): Why FOUC the background (borders, etc.) of iframes and
    // composited layers? Seems like a bug.
    EXPECT_EQ(3, frame2.drawCount());
    EXPECT_TRUE(frame2.contains(SimCanvas::Rect, "white"));
    EXPECT_TRUE(frame2.contains(SimCanvas::Rect, "pink"));
    EXPECT_TRUE(frame2.contains(SimCanvas::Rect, "green"));

    // Finish loading the sheets in the child frame. After it should issue a
    // paint invalidation for every layer since frame2 painted them but skipped
    // painting the real content to avoid FOUC.
    cssResource.complete();

    // First frame where all frames are loaded, should paint the text in the
    // child frame.
    auto frame3 = compositor().beginFrame();
    EXPECT_TRUE(frame3.contains(SimCanvas::Text));
}

TEST_F(DocumentLoadingRenderingTest, ShouldContinuePaintingWhenSheetsStartedAfterBody)
{
    SimRequest mainResource("https://example.com/test.html", "text/html");
    SimRequest cssHeadResource("https://example.com/testHead.css", "text/css");
    SimRequest cssBodyResource("https://example.com/testBody.css", "text/css");

    loadURL("https://example.com/test.html");

    mainResource.start();

    // Still in the head, should not paint.
    mainResource.write("<!DOCTYPE html><link rel=stylesheet href=testHead.css>");
    EXPECT_FALSE(document().isRenderingReady());

    // Sheet is streaming in, but not ready yet.
    cssHeadResource.start();
    cssHeadResource.write("a { color: red; }");
    EXPECT_FALSE(document().isRenderingReady());

    // Body inserted but sheet is still pending so don't paint.
    mainResource.write("<body>");
    EXPECT_FALSE(document().isRenderingReady());

    // Sheet finished and body inserted, ok to paint.
    cssHeadResource.finish();
    EXPECT_TRUE(document().isRenderingReady());

    // In the body, should not stop painting.
    mainResource.write("<link rel=stylesheet href=testBody.css>");
    EXPECT_TRUE(document().isRenderingReady());

    // Finish loading the CSS resource (no change to painting).
    cssBodyResource.complete("a { color: red; }");
    EXPECT_TRUE(document().isRenderingReady());

    // Finish the load, painting should stay enabled.
    mainResource.finish();
    EXPECT_TRUE(document().isRenderingReady());
}


} // namespace blink
