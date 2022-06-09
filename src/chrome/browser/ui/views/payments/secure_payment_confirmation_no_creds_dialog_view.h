// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/secure_payment_confirmation_no_creds_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace payments {

// Draws the user interface in the secure payment confirmation no matching
// credentials flow.
class SecurePaymentConfirmationNoCredsDialogView
    : public SecurePaymentConfirmationNoCredsView,
      public views::DialogDelegateView {
 public:
  METADATA_HEADER(SecurePaymentConfirmationNoCredsDialogView);

  class ObserverForTest {
   public:
    virtual void OnDialogOpened() = 0;
    virtual void OnDialogClosed() = 0;
  };

  // IDs that identify a view within the secure payment confirmation no creds
  // dialog. Used to validate views in browsertests.
  enum class DialogViewID : int {
    VIEW_ID_NONE = 0,
    HEADER_IMAGE,
    PROGRESS_BAR,
    NO_MATCHING_CREDS_TEXT
  };

  explicit SecurePaymentConfirmationNoCredsDialogView(
      ObserverForTest* observer_for_test);
  ~SecurePaymentConfirmationNoCredsDialogView() override;

  SecurePaymentConfirmationNoCredsDialogView(
      const SecurePaymentConfirmationNoCredsDialogView& other) = delete;
  SecurePaymentConfirmationNoCredsDialogView& operator=(
      const SecurePaymentConfirmationNoCredsDialogView& other) = delete;

  // SecurePaymentConfirmationNoCredsView:
  void ShowDialog(content::WebContents* web_contents,
                  const std::u16string& no_creds_text,
                  ResponseCallback response_callback) override;
  void HideDialog() override;

  // views::DialogDelegate:
  bool ShouldShowCloseButton() const override;

  base::WeakPtr<SecurePaymentConfirmationNoCredsDialogView> GetWeakPtr();

 private:
  void OnDialogClosed();

  void InitChildViews(const std::u16string& no_creds_text);
  std::unique_ptr<views::View> CreateBodyView(
      const std::u16string& no_creds_text);

  // May be null.
  raw_ptr<ObserverForTest> observer_for_test_ = nullptr;

  ResponseCallback response_callback_;

  base::WeakPtrFactory<SecurePaymentConfirmationNoCredsDialogView>
      weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_DIALOG_VIEW_H_
