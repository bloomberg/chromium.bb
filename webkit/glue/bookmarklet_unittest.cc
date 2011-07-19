// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

class BookmarkletTest : public TestShellTest {
 public:
  virtual void SetUp() {
    TestShellTest::SetUp();

    test_shell_->LoadURL(GURL("data:text/html,start page"));
    test_shell_->WaitTestFinished();
  }
};

TEST_F(BookmarkletTest, Redirect) {
  test_shell_->LoadURL(
      GURL("javascript:location.href='data:text/plain,SUCCESS'"));
  test_shell_->WaitTestFinished();
  string16 text = test_shell_->GetDocumentText();
  EXPECT_EQ("SUCCESS", UTF16ToASCII(text));
}

TEST_F(BookmarkletTest, RedirectVoided) {
  // This test should be redundant with the Redirect test above.  The point
  // here is to emphasize that in either case the assignment to location during
  // the evaluation of the script should suppress loading the script result.
  // Here, because of the void() wrapping there is no script result.
  test_shell_->LoadURL(
      GURL("javascript:void(location.href='data:text/plain,SUCCESS')"));
  test_shell_->WaitTestFinished();
  string16 text = test_shell_->GetDocumentText();
  EXPECT_EQ("SUCCESS", UTF16ToASCII(text));
}

TEST_F(BookmarkletTest, NonEmptyResult) {
  string16 text;

  // TODO(darin): This test fails in a JSC build.  WebCore+JSC does not really
  // need to support this usage until WebCore supports javascript: URLs that
  // generate content (https://bugs.webkit.org/show_bug.cgi?id=14959).  It is
  // important to note that Safari does not support bookmarklets, and this is
  // really an edge case.  Our behavior with V8 is consistent with FF and IE.
#if 0
  test_shell_->LoadURL(L"javascript:false");
  MessageLoop::current()->RunAllPending();
  text = test_shell_->GetDocumentText();
  EXPECT_EQ("false", UTF16ToASCII(text));
#endif

  test_shell_->LoadURL(GURL("javascript:'hello world'"));
  MessageLoop::current()->RunAllPending();
  text = test_shell_->GetDocumentText();
  EXPECT_EQ("hello world", UTF16ToASCII(text));
}

TEST_F(BookmarkletTest, DocumentWrite) {
  test_shell_->LoadURL(GURL(
      "javascript:document.open();"
      "document.write('hello world');"
      "document.close()"));
  MessageLoop::current()->RunAllPending();
  string16 text = test_shell_->GetDocumentText();
  EXPECT_EQ("hello world", UTF16ToASCII(text));
}

}  // namespace
