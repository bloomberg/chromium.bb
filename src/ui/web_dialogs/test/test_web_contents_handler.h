// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEB_DIALOGS_TEST_TEST_WEB_CONTENTS_HANDLER_H_
#define UI_WEB_DIALOGS_TEST_TEST_WEB_CONTENTS_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

namespace ui {
namespace test {

class TestWebContentsHandler
    : public WebDialogWebContentsDelegate::WebContentsHandler {
 public:
  TestWebContentsHandler();
  ~TestWebContentsHandler() override;

 private:
  // Overridden from WebDialogWebContentsDelegate::WebContentsHandler:
  content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::BrowserContext* context,
                      content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture) override;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsHandler);
};

}  // namespace test
}  // namespace ui

#endif  // UI_WEB_DIALOGS_TEST_TEST_WEB_CONTENTS_HANDLER_H_
