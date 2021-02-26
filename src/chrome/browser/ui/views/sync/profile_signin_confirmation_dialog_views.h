// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;
class Profile;

// A tab-modal dialog to allow a user signing in with a managed account
// to create a new Chrome profile.
class ProfileSigninConfirmationDialogViews : public views::DialogDelegateView {
 public:
  // Create and show the dialog, which owns itself.
  static void ShowDialog(
      Browser* browser,
      Profile* profile,
      const std::string& username,
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate);

  ProfileSigninConfirmationDialogViews(
      Browser* browser,
      const std::string& username,
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
      bool prompt_for_new_profile);
  ~ProfileSigninConfirmationDialogViews() override;

 private:
  static void Show(
      Browser* browser,
      const std::string& username,
      std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate,
      bool prompt_for_new_profile);

  // views::DialogDelegateView:
  ui::ModalType GetModalType() const override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  void ContinueSigninButtonPressed();

  // Called when the "learn more" link is clicked.
  void LearnMoreClicked(const ui::Event& event);

  // Weak ptr to parent view.
  Browser* const browser_;

  // The GAIA username being signed in.
  std::string username_;

  // Dialog button handler.
  std::unique_ptr<ui::ProfileSigninConfirmationDelegate> delegate_;

  // Whether the user should be prompted to create a new profile.
  const bool prompt_for_new_profile_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSigninConfirmationDialogViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SYNC_PROFILE_SIGNIN_CONFIRMATION_DIALOG_VIEWS_H_
