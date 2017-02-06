// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLDocumentParser.h"

#include "core/dom/Document.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

namespace blink {

using namespace HTMLNames;

class HTMLDocumentParserSimTest : public SimTest {
 protected:
  HTMLDocumentParserSimTest() {
    Document::setThreadedParsingEnabledForTesting(true);
  }
  HistogramTester m_histogram;
};

class HTMLDocumentParserLoadingTest
    : public HTMLDocumentParserSimTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  HTMLDocumentParserLoadingTest() {
    Document::setThreadedParsingEnabledForTesting(GetParam());
  }
};

INSTANTIATE_TEST_CASE_P(Threaded,
                        HTMLDocumentParserLoadingTest,
                        ::testing::Values(true));
INSTANTIATE_TEST_CASE_P(NotThreaded,
                        HTMLDocumentParserLoadingTest,
                        ::testing::Values(false));

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldNotPauseParsingForExternalStylesheetsInHead) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssHeadResource("https://example.com/testHead.css", "text/css");

  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<html><head>"
      "<link rel=stylesheet href=testHead.css>"
      "</head><body>"
      "<div id=\"bodyDiv\"></div>"
      "</body></html>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("bodyDiv"));
  cssHeadResource.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("bodyDiv"));
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldNotPauseParsingForExternalStylesheetsImportedInHead) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssHeadResource("https://example.com/testHead.css", "text/css");

  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<html><head>"
      "<style>"
      "@import 'testHead.css'"
      "</style>"
      "</head><body>"
      "<div id=\"bodyDiv\"></div>"
      "</body></html>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("bodyDiv"));
  cssHeadResource.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("bodyDiv"));
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldPauseParsingForExternalStylesheetsInBody) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssHeadResource("https://example.com/testHead.css", "text/css");
  SimRequest cssBodyResource("https://example.com/testBody.css", "text/css");

  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<html><head>"
      "<link rel=stylesheet href=testHead.css>"
      "</head><body>"
      "<div id=\"before\"></div>"
      "<link rel=stylesheet href=testBody.css>"
      "<div id=\"after\"></div>"
      "</body></html>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after"));

  // Completing the head css shouldn't change anything
  cssHeadResource.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after"));

  // Completing the body resource and pumping the tasks should continue parsing
  // and create the "after" div.
  cssBodyResource.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_TRUE(document().getElementById("after"));
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldPauseParsingForExternalStylesheetsInBodyIncremental) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssHeadResource("https://example.com/testHead.css", "text/css");
  SimRequest cssBodyResource1("https://example.com/testBody1.css", "text/css");
  SimRequest cssBodyResource2("https://example.com/testBody2.css", "text/css");
  SimRequest cssBodyResource3("https://example.com/testBody3.css", "text/css");

  loadURL("https://example.com/test.html");

  mainResource.start();
  mainResource.write(
      "<!DOCTYPE html>"
      "<html><head>"
      "<link rel=stylesheet href=testHead.css>"
      "</head><body>"
      "<div id=\"before\"></div>"
      "<link rel=stylesheet href=testBody1.css>"
      "<div id=\"after1\"></div>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after1"));
  EXPECT_FALSE(document().getElementById("after2"));
  EXPECT_FALSE(document().getElementById("after3"));

  mainResource.write(
      "<link rel=stylesheet href=testBody2.css>"
      "<div id=\"after2\"></div>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after1"));
  EXPECT_FALSE(document().getElementById("after2"));
  EXPECT_FALSE(document().getElementById("after3"));

  mainResource.complete(
      "<link rel=stylesheet href=testBody3.css>"
      "<div id=\"after3\"></div>"
      "</body></html>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after1"));
  EXPECT_FALSE(document().getElementById("after2"));
  EXPECT_FALSE(document().getElementById("after3"));

  // Completing the head css shouldn't change anything
  cssHeadResource.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after1"));
  EXPECT_FALSE(document().getElementById("after2"));
  EXPECT_FALSE(document().getElementById("after3"));

  // Completing the second css shouldn't change anything
  cssBodyResource2.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after1"));
  EXPECT_FALSE(document().getElementById("after2"));
  EXPECT_FALSE(document().getElementById("after3"));

  // Completing the first css should allow the parser to continue past it and
  // the second css which was already completed and then pause again before the
  // third css.
  cssBodyResource1.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_TRUE(document().getElementById("after1"));
  EXPECT_TRUE(document().getElementById("after2"));
  EXPECT_FALSE(document().getElementById("after3"));

  // Completing the third css should let it continue to the end.
  cssBodyResource3.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_TRUE(document().getElementById("after1"));
  EXPECT_TRUE(document().getElementById("after2"));
  EXPECT_TRUE(document().getElementById("after3"));
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldNotPauseParsingForExternalNonMatchingStylesheetsInBody) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssHeadResource("https://example.com/testHead.css", "text/css");

  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<html><head>"
      "<link rel=stylesheet href=testHead.css>"
      "</head><body>"
      "<div id=\"before\"></div>"
      "<link rel=stylesheet href=testBody.css type='print'>"
      "<div id=\"after\"></div>"
      "</body></html>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_TRUE(document().getElementById("after"));

  // Completing the head css shouldn't change anything
  cssHeadResource.complete("");
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldPauseParsingForExternalStylesheetsImportedInBody) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssHeadResource("https://example.com/testHead.css", "text/css");
  SimRequest cssBodyResource("https://example.com/testBody.css", "text/css");

  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<html><head>"
      "<link rel=stylesheet href=testHead.css>"
      "</head><body>"
      "<div id=\"before\"></div>"
      "<style>"
      "@import 'testBody.css'"
      "</style>"
      "<div id=\"after\"></div>"
      "</body></html>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after"));

  // Completing the head css shouldn't change anything
  cssHeadResource.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after"));

  // Completing the body resource and pumping the tasks should continue parsing
  // and create the "after" div.
  cssBodyResource.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_TRUE(document().getElementById("after"));
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldPauseParsingForExternalStylesheetsWrittenInBody) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssHeadResource("https://example.com/testHead.css", "text/css");
  SimRequest cssBodyResource("https://example.com/testBody.css", "text/css");

  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<html><head>"
      "<link rel=stylesheet href=testHead.css>"
      "</head><body>"
      "<div id=\"before\"></div>"
      "<script>"
      "document.write('<link rel=stylesheet href=testBody.css>');"
      "</script>"
      "<div id=\"after\"></div>"
      "</body></html>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after"));

  // Completing the head css shouldn't change anything
  cssHeadResource.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_FALSE(document().getElementById("after"));

  // Completing the body resource and pumping the tasks should continue parsing
  // and create the "after" div.
  cssBodyResource.complete("");
  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_TRUE(document().getElementById("after"));
}

TEST_P(HTMLDocumentParserLoadingTest,
       PendingHeadStylesheetShouldNotBlockParserForBodyInlineStyle) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssHeadResource("https://example.com/testHead.css", "text/css");

  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<html><head>"
      "<link rel=stylesheet href=testHead.css>"
      "</head><body>"
      "<div id=\"before\"></div>"
      "<style>"
      "</style>"
      "<div id=\"after\"></div>"
      "</body></html>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_TRUE(document().getElementById("after"));
  cssHeadResource.complete("");
}

TEST_P(HTMLDocumentParserLoadingTest,
       PendingHeadStylesheetShouldNotBlockParserForBodyShadowDom) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssHeadResource("https://example.com/testHead.css", "text/css");

  loadURL("https://example.com/test.html");

  // The marquee tag has a shadow DOM that synchronously applies a stylesheet.
  mainResource.complete(
      "<!DOCTYPE html>"
      "<html><head>"
      "<link rel=stylesheet href=testHead.css>"
      "</head><body>"
      "<div id=\"before\"></div>"
      "<marquee>Marquee</marquee>"
      "<div id=\"after\"></div>"
      "</body></html>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_TRUE(document().getElementById("after"));
  cssHeadResource.complete("");
}

TEST_P(HTMLDocumentParserLoadingTest,
       ShouldNotPauseParsingForExternalStylesheetsAttachedInBody) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  SimRequest cssAsyncResource("https://example.com/testAsync.css", "text/css");

  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<html><head>"
      "</head><body>"
      "<div id=\"before\"></div>"
      "<script>"
      "var attach  = document.getElementsByTagName('script')[0];"
      "var link  = document.createElement('link');"
      "link.rel  = 'stylesheet';"
      "link.type = 'text/css';"
      "link.href = 'testAsync.css';"
      "link.media = 'all';"
      "attach.appendChild(link);"
      "</script>"
      "<div id=\"after\"></div>"
      "</body></html>");

  testing::runPendingTasks();
  EXPECT_TRUE(document().getElementById("before"));
  EXPECT_TRUE(document().getElementById("after"));

  cssAsyncResource.complete("");
}

TEST_F(HTMLDocumentParserSimTest, NoRewindNoDocWrite) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<html><body>no doc write"
      "</body></html>");

  testing::runPendingTasks();
  m_histogram.expectTotalCount("Parser.DiscardedTokenCount", 0);
}

TEST_F(HTMLDocumentParserSimTest, RewindBrokenToken) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<script>"
      "document.write('<a');"
      "</script>");

  testing::runPendingTasks();
  m_histogram.expectTotalCount("Parser.DiscardedTokenCount", 1);
}

TEST_F(HTMLDocumentParserSimTest, RewindDifferentNamespace) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<script>"
      "document.write('<svg>');"
      "</script>");

  testing::runPendingTasks();
  m_histogram.expectTotalCount("Parser.DiscardedTokenCount", 1);
}

TEST_F(HTMLDocumentParserSimTest, NoRewindSaneDocWrite1) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<script>"
      "document.write('<script>console.log(\'hello world\');<\\/script>');"
      "</script>");

  testing::runPendingTasks();
  m_histogram.expectTotalCount("Parser.DiscardedTokenCount", 0);
}

TEST_F(HTMLDocumentParserSimTest, NoRewindSaneDocWrite2) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<script>"
      "document.write('<p>hello world<\\/p><a>yo');"
      "</script>");

  testing::runPendingTasks();
  m_histogram.expectTotalCount("Parser.DiscardedTokenCount", 0);
}

TEST_F(HTMLDocumentParserSimTest, NoRewindSaneDocWriteWithTitle) {
  SimRequest mainResource("https://example.com/test.html", "text/html");
  loadURL("https://example.com/test.html");

  mainResource.complete(
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "<title></title>"
      "<script>document.write('<p>testing');</script>"
      "</head>"
      "<body>"
      "</body>"
      "</html>");

  testing::runPendingTasks();
  m_histogram.expectTotalCount("Parser.DiscardedTokenCount", 0);
}

}  // namespace blink
