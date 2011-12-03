// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/string_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

typedef TestShellTest IFrameRedirectTest;

// Tests that loading a page in an iframe from javascript results in
// a redirect from about:blank.
TEST_F(IFrameRedirectTest, Test) {
  FilePath iframes_data_dir_ = data_dir_;
  iframes_data_dir_ = iframes_data_dir_.AppendASCII("test_shell");
  iframes_data_dir_ = iframes_data_dir_.AppendASCII("iframe_redirect");
  ASSERT_TRUE(file_util::PathExists(iframes_data_dir_));

  GURL test_url = GetTestURL(iframes_data_dir_, "main.html");

  test_shell_->LoadURL(test_url);
  test_shell_->WaitTestFinished();

  WebFrame* iframe =
      test_shell_->webView()->findFrameByName(WebString::fromUTF8("ifr"));
  ASSERT_TRUE(iframe != NULL);
  WebDataSource* iframe_ds = iframe->dataSource();
  ASSERT_TRUE(iframe_ds != NULL);
  WebVector<WebURL> redirects;
  iframe_ds->redirectChain(redirects);
  ASSERT_FALSE(redirects.isEmpty());
  ASSERT_TRUE(GURL(redirects[0]) == GURL("about:blank"));
}
