// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/string_util.h"
#include "net/url_request/url_request_test_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/unittest_test_server.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebFrame;

namespace {

class MimeTypeTests : public TestShellTest {
 public:
  void LoadURL(const GURL& url) {
    test_shell_->LoadURL(url);
    test_shell_->WaitTestFinished();
  }

  void CheckMimeType(const char* mimetype, const std::string& expected) {
    std::string path("contenttype?");
    GURL url(test_server_.GetURL(path + mimetype));
    LoadURL(url);
    WebFrame* frame = test_shell_->webView()->mainFrame();
    EXPECT_EQ(expected, UTF16ToASCII(webkit_glue::DumpDocumentText(frame)));
  }

  UnittestTestServer test_server_;
};

TEST_F(MimeTypeTests, MimeTypeTests) {
  ASSERT_TRUE(test_server_.Start());

  std::string expected_src("<html>\n<body>\n"
      "<p>HTML text</p>\n</body>\n</html>\n");

  // These files should all be displayed as plain text.
  const char* plain_text[] = {
    // It is unclear whether to display text/css or download it.
    //   Firefox 3: Display
    //   Internet Explorer 7: Download
    //   Safari 3.2: Download
    // We choose to match Firefox due to the lot of complains
    // from the users if css files are downloaded:
    // http://code.google.com/p/chromium/issues/detail?id=7192
    "text/css",
    "text/javascript",
    "text/plain",
    "application/x-javascript",
  };
  for (size_t i = 0; i < arraysize(plain_text); ++i) {
    CheckMimeType(plain_text[i], expected_src);
  }

  // These should all be displayed as html content.
  const char* html_src[] = {
    "text/html",
    "text/xml",
    "text/xsl",
    "application/xhtml+xml",
  };
  for (size_t i = 0; i < arraysize(html_src); ++i) {
    CheckMimeType(html_src[i], "HTML text");
  }

  // These shouldn't be rendered as text or HTML, but shouldn't download
  // either.
  const char* not_text[] = {
    "image/png",
    "image/gif",
    "image/jpeg",
    "image/bmp",
  };
  for (size_t i = 0; i < arraysize(not_text); ++i) {
    CheckMimeType(not_text[i], "");
    test_shell_->webView()->mainFrame()->stopLoading();
  }

  // TODO(tc): make sure other mime types properly go to download (e.g.,
  // image/foo).

}

}  // namespace
