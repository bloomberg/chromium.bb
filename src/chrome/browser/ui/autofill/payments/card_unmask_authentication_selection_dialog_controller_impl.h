// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_authentication_selection_dialog_controller.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class CardUnmaskAuthenticationSelectionDialogView;

class CardUnmaskAuthenticationSelectionDialogControllerImpl
    : public CardUnmaskAuthenticationSelectionDialogController,
      public content::WebContentsUserData<
          CardUnmaskAuthenticationSelectionDialogControllerImpl> {
 public:
  CardUnmaskAuthenticationSelectionDialogControllerImpl(
      const CardUnmaskAuthenticationSelectionDialogControllerImpl&) = delete;
  CardUnmaskAuthenticationSelectionDialogControllerImpl& operator=(
      const CardUnmaskAuthenticationSelectionDialogControllerImpl&) = delete;
  ~CardUnmaskAuthenticationSelectionDialogControllerImpl() override;

  // Get the controller instance given the |web_contents|. If it does not exist,
  // create one.
  static CardUnmaskAuthenticationSelectionDialogControllerImpl* GetOrCreate(
      content::WebContents* web_contents);

  void ShowDialog(
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      base::OnceCallback<void(const std::string&)>
          confirm_unmasking_method_callback,
      base::OnceClosure cancel_unmasking_closure);
  // Called when we receive a server response after the user accepts (clicks the
  // ok button) on a challenge option. |server_success| represents a successful
  // server response, where true means success and false means an error was
  // returned by the server.
  void DismissDialogUponServerProcessedAuthenticationMethodRequest(
      bool server_success);

  // CardUnmaskAuthenticationSelectionDialogController:
  void OnDialogClosed(bool user_closed_dialog, bool server_success) override;
  void OnOkButtonClicked(
      const std::string& selected_challenge_option_id) override;
  std::u16string GetWindowTitle() const override;
  std::u16string GetContentHeaderText() const override;
  const std::vector<CardUnmaskChallengeOption>& GetChallengeOptions()
      const override;
  ui::ImageModel GetAuthenticationModeIcon(
      const CardUnmaskChallengeOption& challenge_option) const override;
  std::u16string GetAuthenticationModeLabel(
      const CardUnmaskChallengeOption& challenge_option) const override;
  std::u16string GetContentFooterText() const override;
  std::u16string GetOkButtonLabel() const override;
  std::u16string GetProgressLabel() const override;

#if defined(UNIT_TEST)
  CardUnmaskAuthenticationSelectionDialogView* GetDialogViewForTesting() {
    return dialog_view_;
  }
#endif

 protected:
  explicit CardUnmaskAuthenticationSelectionDialogControllerImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      CardUnmaskAuthenticationSelectionDialogControllerImpl>;

  // Contains all of the challenge options an issuer has for the user.
  std::vector<CardUnmaskChallengeOption> challenge_options_;

  raw_ptr<CardUnmaskAuthenticationSelectionDialogView> dialog_view_ = nullptr;

  // Callback invoked when the user confirmed an authentication method to use.
  base::OnceCallback<void(const std::string&)>
      confirm_unmasking_method_callback_;

  // Callback invoked when the user cancelled the card unmasking.
  base::OnceClosure cancel_unmasking_closure_;

  // Tracks whether a challenge option was selected in the current
  // |dialog_view_|.
  bool challenge_option_selected_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CONTROLLER_IMPL_H_
