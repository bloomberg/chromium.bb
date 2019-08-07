// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"

namespace {

class ChromeConstrainedWindowViewsClient
    : public constrained_window::ConstrainedWindowViewsClient {
 public:
  ChromeConstrainedWindowViewsClient() {}
  ~ChromeConstrainedWindowViewsClient() override {}

 private:
  // ConstrainedWindowViewsClient:
  web_modal::ModalDialogHost* GetModalDialogHost(
      gfx::NativeWindow parent) override {
    // Get the browser dialog management and hosting components from |parent|.
    Browser* browser = chrome::FindBrowserWithWindow(parent);
    if (browser) {
      ChromeWebModalDialogManagerDelegate* manager = browser;
      return manager->GetWebContentsModalDialogHost();
    }
    return nullptr;
  }
  gfx::NativeView GetDialogHostView(gfx::NativeWindow parent) override {
    return platform_util::GetViewForWindow(parent);
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeConstrainedWindowViewsClient);
};

}  // namespace

std::unique_ptr<constrained_window::ConstrainedWindowViewsClient>
CreateChromeConstrainedWindowViewsClient() {
  return base::WrapUnique(new ChromeConstrainedWindowViewsClient);
}
