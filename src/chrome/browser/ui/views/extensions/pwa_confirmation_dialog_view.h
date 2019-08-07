// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_DIALOG_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/extensions/pwa_confirmation.h"
#include "chrome/common/web_application_info.h"
#include "ui/views/window/dialog_delegate.h"

// PWAConfirmationDialogView provides a dialog for accepting or rejecting the
// installation of a PWA (Progressive Web App).
class PWAConfirmationDialogView : public views::DialogDelegateView {
 public:
  // Constructs a PWAConfirmationDialogView. |web_app_info| contains information
  // about a web app that has passed the PWA check.
  PWAConfirmationDialogView(std::unique_ptr<WebApplicationInfo> web_app_info,
                            chrome::AppInstallationAcceptanceCallback callback);
  ~PWAConfirmationDialogView() override;

 private:
  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  views::View* GetInitiallyFocusedView() override;
  void WindowClosing() override;

  // views::DialogDelegateView:
  bool Accept() override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  PWAConfirmation pwa_confirmation_;

  DISALLOW_COPY_AND_ASSIGN(PWAConfirmationDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_DIALOG_VIEW_H_
