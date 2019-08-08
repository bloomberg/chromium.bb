// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_BROWSERTEST_H_

#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/web_ui_message_handler.h"

class BookmarksBrowserTest : public WebUIBrowserTest,
                             public content::WebUIMessageHandler {
 public:
  BookmarksBrowserTest();
  ~BookmarksBrowserTest() override;

  void SetIncognitoAvailability(int availability);
  void SetCanEditBookmarks(bool canEdit);

  void SetupExtensionAPITest();
  void SetupExtensionAPIEditDisabledTest();

 private:
  void HandleSetIncognitoAvailability(const base::ListValue* args);
  void HandleSetCanEditBookmarks(const base::ListValue* args);

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // WebUIBrowserTest:
  content::WebUIMessageHandler* GetMockMessageHandler() override;

  DISALLOW_COPY_AND_ASSIGN(BookmarksBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_BOOKMARKS_BOOKMARKS_BROWSERTEST_H_
