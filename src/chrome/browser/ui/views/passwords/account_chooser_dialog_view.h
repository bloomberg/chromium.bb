// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_ACCOUNT_CHOOSER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_ACCOUNT_CHOOSER_DIALOG_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"

namespace content {
class WebContents;
}

class CredentialManagerDialogController;

class AccountChooserDialogView : public views::BubbleDialogDelegateView,
                                 public views::ButtonListener,
                                 public AccountChooserPrompt {
 public:
  AccountChooserDialogView(CredentialManagerDialogController* controller,
                           content::WebContents* web_contents);
  ~AccountChooserDialogView() override;

  // AccountChooserPrompt:
  void ShowAccountChooser() override;
  void ControllerGone() override;

 private:
  // WidgetDelegate:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;

  // DialogDelegate:
  bool Accept() override;

  // ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Sets up the child views.
  void InitWindow();

  // A weak pointer to the controller.
  CredentialManagerDialogController* controller_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_ACCOUNT_CHOOSER_DIALOG_VIEW_H_
