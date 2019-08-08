// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/web_application_info.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class View;
}

// Provides the core UI for confirming the installation of a PWA for the
// |PWAConfirmationDialogView| and |PWAConfirmationBubbleView| form factors.
class PWAConfirmation {
 public:
  static base::string16 GetWindowTitle();
  static base::string16 GetDialogButtonLabel(ui::DialogButton button);
  static views::View* GetInitiallyFocusedView(
      views::DialogDelegateView* dialog);

  PWAConfirmation(views::DialogDelegateView* dialog,
                  std::unique_ptr<WebApplicationInfo> web_app_info,
                  chrome::AppInstallationAcceptanceCallback callback);
  ~PWAConfirmation();

  void Accept();
  void WindowClosing();

 private:
  // The WebApplicationInfo that the user is confirming.
  // Cleared when the dialog completes (Accept/WindowClosing).
  std::unique_ptr<WebApplicationInfo> web_app_info_;
  chrome::AppInstallationAcceptanceCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PWAConfirmation);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_PWA_CONFIRMATION_H_
