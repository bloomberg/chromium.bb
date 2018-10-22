// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_DIALOG_TEST_BROWSER_WINDOW_H_
#define CHROME_TEST_BASE_DIALOG_TEST_BROWSER_WINDOW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"

namespace views {
class Widget;
}

class Browser;

// Custom test browser window to provide a parent view to a modal dialog.
class DialogTestBrowserWindow : public TestBrowserWindow,
                                public web_modal::WebContentsModalDialogHost {
 public:
  DialogTestBrowserWindow();
  ~DialogTestBrowserWindow() override;

  // BrowserWindow overrides
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost overrides
  gfx::NativeView GetHostView() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 private:
  Browser* FindBrowser() const;

  // Dummy window for parenting dialogs.
  std::unique_ptr<views::Widget> host_window_;

  DISALLOW_COPY_AND_ASSIGN(DialogTestBrowserWindow);
};

#endif  // CHROME_TEST_BASE_DIALOG_TEST_BROWSER_WINDOW_H_
