// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test.h"

#include "content/public/test/web_contents_tester.h"

namespace web_app {

WebAppTest::WebAppTest() = default;

WebAppTest::~WebAppTest() = default;

void WebAppTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SetContents(CreateTestWebContents());
}

}  // namespace web_app
