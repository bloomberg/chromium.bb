// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell_test.h"

class WebViewTest : public TestShellTest {
};

TEST_F(WebViewTest, GetContentAsPlainText) {
  WebView* view = test_shell_->webView();
  ASSERT_TRUE(view != 0);

  view->SetActive(true);
  EXPECT_TRUE(view->IsActive());

  view->SetActive(false);
  EXPECT_FALSE(view->IsActive());

  view->SetActive(true);
  EXPECT_TRUE(view->IsActive());
}

// TODO(viettrungluu): add more tests
