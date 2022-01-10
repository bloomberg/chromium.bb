// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_view.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// Implementation of the AutofillProgressDialogController. This class shows a
// progress bar with a cancel button that can be updated to a success state
// (check mark). The controller is destroyed once the view is dismissed.
class AutofillProgressDialogControllerImpl
    : public AutofillProgressDialogController {
 public:
  explicit AutofillProgressDialogControllerImpl(
      content::WebContents* web_contents);

  AutofillProgressDialogControllerImpl(
      const AutofillProgressDialogControllerImpl&) = delete;
  AutofillProgressDialogControllerImpl& operator=(
      const AutofillProgressDialogControllerImpl&) = delete;

  ~AutofillProgressDialogControllerImpl() override;

  void ShowDialog(base::OnceClosure cancel_callback);
  void DismissDialog(bool show_confirmation_before_closing);

  // AutofillProgressDialogController.
  void OnDismissed(bool is_canceled_by_user) override;
  const std::u16string GetTitle() override;
  const std::u16string GetCancelButtonLabel() override;
  const std::u16string GetLoadingMessage() override;
  const std::u16string GetConfirmationMessage() override;

  content::WebContents* GetWebContents() override;

  AutofillProgressDialogView* autofill_progress_dialog_view() {
    return autofill_progress_dialog_view_;
  }

 private:
  raw_ptr<content::WebContents> web_contents_;

  // View that displays the error dialog.
  raw_ptr<AutofillProgressDialogView> autofill_progress_dialog_view_ = nullptr;

  // Callback function invoked when the cancel button is clicked.
  base::OnceClosure cancel_callback_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_PROGRESS_DIALOG_CONTROLLER_IMPL_H_
