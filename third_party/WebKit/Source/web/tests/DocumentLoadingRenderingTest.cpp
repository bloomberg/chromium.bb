// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/dom/Document.h"
#include "core/page/Page.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/sim/SimLayerTreeView.h"
#include "web/tests/sim/SimNetwork.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimWebViewClient.h"
#include <gtest/gtest.h>

namespace blink {

class DocumentLoadingRenderingTest : public ::testing::Test {
protected:
    DocumentLoadingRenderingTest()
        : m_webViewClient(m_layerTreeView)
    {
        m_webViewHelper.initialize(true, nullptr, &m_webViewClient);
        Document::setThreadedParsingEnabledForUnitTestsOnly(false);
    }

    virtual ~DocumentLoadingRenderingTest()
    {
        Document::setThreadedParsingEnabledForUnitTestsOnly(true);
    }

    void loadURL(const String& url)
    {
        WebURLRequest request;
        request.initialize();
        request.setURL(KURL(ParsedURLString, url));
        m_webViewHelper.webViewImpl()->mainFrameImpl()->loadRequest(request);
    }

    Document& document() { return *toLocalFrame(m_webViewHelper.webViewImpl()->page()->mainFrame())->document(); }

    SimNetwork m_network;
    SimLayerTreeView m_layerTreeView;
    SimWebViewClient m_webViewClient;
    FrameTestHelpers::WebViewHelper m_webViewHelper;
};

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterBodyParsedWithoutSheets)
{
    SimRequest mainResource("https://example.com/test.html", "text/html");

    loadURL("https://example.com/test.html");

    mainResource.start();

    // Still in the head, should not resume commits.
    mainResource.write("<!DOCTYPE html>");
    EXPECT_TRUE(m_layerTreeView.deferCommits());
    mainResource.write("<title>Test</title><style>div { color red; }</style>");
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Implicitly inserts the body. Since there's no loading stylesheets we
    // should resume commits.
    mainResource.write("<p>Hello World</p>");
    EXPECT_FALSE(m_layerTreeView.deferCommits());

    // Finish the load, should stay resumed.
    mainResource.finish();
    EXPECT_FALSE(m_layerTreeView.deferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterBodyIfSheetsLoaded)
{
    SimRequest mainResource("https://example.com/test.html", "text/html");
    SimRequest cssResource("https://example.com/test.css", "text/css");

    loadURL("https://example.com/test.html");

    mainResource.start();

    // Still in the head, should not resume commits.
    mainResource.write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Sheet is streaming in, but not ready yet.
    cssResource.start();
    cssResource.write("a { color: red; }");
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Sheet finished, but no body yet, so don't resume.
    cssResource.finish();
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Body inserted and sheet is loaded so resume commits.
    mainResource.write("<body>");
    EXPECT_FALSE(m_layerTreeView.deferCommits());

    // Finish the load, should stay resumed.
    mainResource.finish();
    EXPECT_FALSE(m_layerTreeView.deferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterSheetsLoaded)
{
    SimRequest mainResource("https://example.com/test.html", "text/html");
    SimRequest cssResource("https://example.com/test.css", "text/css");

    loadURL("https://example.com/test.html");

    mainResource.start();

    // Still in the head, should not resume commits.
    mainResource.write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Sheet is streaming in, but not ready yet.
    cssResource.start();
    cssResource.write("a { color: red; }");
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Body inserted, but sheet is still loading so don't resume.
    mainResource.write("<body>");
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Sheet finished and there's a body so resume.
    cssResource.finish();
    EXPECT_FALSE(m_layerTreeView.deferCommits());

    // Finish the load, should stay resumed.
    mainResource.finish();
    EXPECT_FALSE(m_layerTreeView.deferCommits());
}

TEST_F(DocumentLoadingRenderingTest, ShouldResumeCommitsAfterParsingSvg)
{
    SimRequest mainResource("https://example.com/test.svg", "image/svg+xml");
    SimRequest cssResource("https://example.com/test.css", "text/css");

    loadURL("https://example.com/test.svg");

    mainResource.start();

    // Not done parsing.
    mainResource.write("<?xml-stylesheet type='text/css' href='test.css'?>");
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Sheet is streaming in, but not ready yet.
    cssResource.start();
    cssResource.write("a { color: red; }");
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Root inserted, but sheet is still loading so don't resume.
    mainResource.write("<svg>");
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Sheet finished, but no body since it's svg so don't resume.
    cssResource.finish();
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    // Finish the load and resume.
    mainResource.finish();
    EXPECT_FALSE(m_layerTreeView.deferCommits());
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
    EXPECT_TRUE(m_layerTreeView.deferCommits());

    firstCssResource.start();
    firstCssResource.write("body { color: red; }");
    mainResource.write("<body>");
    firstCssResource.finish();

    // Sheet finished and there's a body so resume.
    EXPECT_FALSE(m_layerTreeView.deferCommits());

    mainResource.finish();
    m_layerTreeView.clearNeedsAnimate();

    // Replace the stylesheet by changing href.
    Element* element = document().getElementById("link");
    EXPECT_NE(nullptr, element);
    element->setAttribute(HTMLNames::hrefAttr, "second.css");
    EXPECT_FALSE(m_layerTreeView.needsAnimate());

    secondCssResource.start();
    secondCssResource.write("body { color: red; }");
    secondCssResource.finish();
    EXPECT_TRUE(m_layerTreeView.needsAnimate());
}

} // namespace blink
