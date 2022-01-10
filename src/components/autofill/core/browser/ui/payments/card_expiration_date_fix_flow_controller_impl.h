// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller.h"

namespace autofill {

class CardExpirationDateFixFlowView;

// Enables the user to accept or deny expiration date fix flow prompt.
// Only used on mobile. This class is responsible for its destruction.
// Destruction is achieved by calling delete when the prompt is
// dismissed.
class CardExpirationDateFixFlowControllerImpl
    : public CardExpirationDateFixFlowController {
 public:
  CardExpirationDateFixFlowControllerImpl();

  CardExpirationDateFixFlowControllerImpl(
      const CardExpirationDateFixFlowControllerImpl&) = delete;
  CardExpirationDateFixFlowControllerImpl& operator=(
      const CardExpirationDateFixFlowControllerImpl&) = delete;

  ~CardExpirationDateFixFlowControllerImpl() override;

  void Show(CardExpirationDateFixFlowView* card_expiration_date_fix_flow_view,
            const CreditCard& card,
            base::OnceCallback<void(const std::u16string&,
                                    const std::u16string&)> callback);

  // CardExpirationDateFixFlowController implementation.
  void OnAccepted(const std::u16string& month,
                  const std::u16string& year) override;
  void OnDismissed() override;
  void OnDialogClosed() override;
  int GetIconId() const override;
  std::u16string GetTitleText() const override;
  std::u16string GetSaveButtonLabel() const override;
  std::u16string GetCardLabel() const override;
  std::u16string GetCancelButtonLabel() const override;
  std::u16string GetInputLabel() const override;
  std::u16string GetDateSeparator() const override;
  std::u16string GetInvalidDateError() const override;

 private:
  // View that displays the fix flow prompt.
  raw_ptr<CardExpirationDateFixFlowView> card_expiration_date_fix_flow_view_ =
      nullptr;

  // The callback to save the credit card to Google Payments once user accepts
  // fix flow.
  base::OnceCallback<void(const std::u16string&, const std::u16string&)>
      upload_save_card_callback_;

  // Whether the prompt was shown to the user.
  bool shown_ = false;

  // Did the user ever explicitly accept or dismiss this prompt?
  bool had_user_interaction_ = false;

  // Label of the card describing the network and the last four digits.
  std::u16string card_label_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_EXPIRATION_DATE_FIX_FLOW_CONTROLLER_IMPL_H_
