// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/web_dialog_view.h"

#include "ash/public/cpp/login_screen.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/dialog_delegate.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace login_screen_extension_ui {

WebDialogView::WebDialogView(
    content::BrowserContext* context,
    DialogDelegate* delegate,
    std::unique_ptr<ui::WebDialogWebContentsDelegate::WebContentsHandler>
        handler)
    : views::WebDialogView(context, delegate, std::move(handler)),
      delegate_(delegate) {
  views::WidgetDelegate::SetShowTitle(!delegate_ ||
                                      delegate_->ShouldCenterDialogTitleText());
  if (LoginScreenClient::HasInstance()) {
    LoginScreenClient::Get()->AddSystemTrayFocusObserver(this);
  }
}

WebDialogView::~WebDialogView() {
  if (LoginScreenClient::HasInstance()) {
    LoginScreenClient::Get()->RemoveSystemTrayFocusObserver(this);
  }
}

bool WebDialogView::ShouldShowCloseButton() const {
  return !delegate_ || delegate_->CanCloseDialog();
}

bool WebDialogView::TakeFocus(content::WebContents* source, bool reverse) {
  ash::LoginScreen::Get()->FocusLoginShelf(reverse);
  return true;
}

void WebDialogView::OnFocusLeavingSystemTray(bool reverse) {
  web_contents()->FocusThroughTabTraversal(reverse);
  web_contents()->Focus();
}

}  // namespace login_screen_extension_ui

}  // namespace chromeos
