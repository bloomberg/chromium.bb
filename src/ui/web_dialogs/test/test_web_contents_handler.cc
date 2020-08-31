// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/test/test_web_contents_handler.h"

#include "content/public/browser/web_contents.h"

namespace ui {
namespace test {

TestWebContentsHandler::TestWebContentsHandler() {
}

TestWebContentsHandler::~TestWebContentsHandler() {
}

content::WebContents* TestWebContentsHandler::OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) {
  return NULL;
}

void TestWebContentsHandler::AddNewContents(
    content::BrowserContext* context,
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture) {}

}  // namespace test
}  // namespace ui
