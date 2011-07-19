// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebView;

class WebFrameTest : public TestShellTest {
};

TEST_F(WebFrameTest, GetContentAsPlainText) {
  WebView* view = test_shell_->webView();
  WebFrame* frame = view->mainFrame();

  // Generate a simple test case.
  const char simple_source[] = "<div>Foo bar</div><div></div>baz";
  GURL test_url("http://foo/");
  frame->loadHTMLString(simple_source, test_url);
  test_shell_->WaitTestFinished();

  // Make sure it comes out OK.
  const string16 expected(ASCIIToUTF16("Foo bar\nbaz"));
  string16 text = frame->contentAsText(std::numeric_limits<size_t>::max());
  EXPECT_EQ(expected, text);

  // Try reading the same one with clipping of the text.
  const int len = 5;
  text = frame->contentAsText(len);
  EXPECT_EQ(expected.substr(0, len), text);

  // Now do a new test with a subframe.
  const char outer_frame_source[] = "Hello<iframe></iframe> world";
  frame->loadHTMLString(outer_frame_source, test_url);
  test_shell_->WaitTestFinished();

  // Load something into the subframe.
  WebFrame* subframe = frame->findChildByExpression(
      WebString::fromUTF8("/html/body/iframe"));
  ASSERT_TRUE(subframe);
  subframe->loadHTMLString("sub<p>text", test_url);
  test_shell_->WaitTestFinished();

  text = frame->contentAsText(std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello world\n\nsub\ntext", UTF16ToUTF8(text));

  // Get the frame text where the subframe separator falls on the boundary of
  // what we'll take. There used to be a crash in this case.
  text = frame->contentAsText(12);
  EXPECT_EQ("Hello world", UTF16ToUTF8(text));
}

TEST_F(WebFrameTest, GetFullHtmlOfPage) {
  WebView* view = test_shell_->webView();
  WebFrame* frame = view->mainFrame();

  // Generate a simple test case.
  const char simple_source[] = "<p>Hello</p><p>World</p>";
  GURL test_url("http://hello/");
  frame->loadHTMLString(simple_source, test_url);
  test_shell_->WaitTestFinished();

  string16 text = frame->contentAsText(std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello\n\nWorld", UTF16ToUTF8(text));

  const std::string html = frame->contentAsMarkup().utf8();

  // Load again with the output html.
  frame->loadHTMLString(html, test_url);
  test_shell_->WaitTestFinished();

  EXPECT_EQ(html, UTF16ToUTF8(frame->contentAsMarkup()));

  text = frame->contentAsText(std::numeric_limits<size_t>::max());
  EXPECT_EQ("Hello\n\nWorld", UTF16ToUTF8(text));

  // Test selection check
  EXPECT_FALSE(frame->hasSelection());
  frame->executeCommand(WebString::fromUTF8("SelectAll"));
  EXPECT_TRUE(frame->hasSelection());
  frame->executeCommand(WebString::fromUTF8("Unselect"));
  EXPECT_FALSE(frame->hasSelection());
  WebString selection_html = frame->selectionAsMarkup();
  EXPECT_TRUE(selection_html.isEmpty());
}
