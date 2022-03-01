// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_H_

#include <string>

#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"

namespace base {
class TimeDelta;
}

namespace autofill {

class CardUnmaskPromptController {
 public:
  // Interaction.
  virtual void OnUnmaskDialogClosed() = 0;
  virtual void OnUnmaskPromptAccepted(const std::u16string& cvc,
                                      const std::u16string& exp_month,
                                      const std::u16string& exp_year,
                                      bool enable_fido_auth) = 0;
  virtual void NewCardLinkClicked() = 0;

  // State.
  virtual std::u16string GetWindowTitle() const = 0;
  virtual std::u16string GetInstructionsMessage() const = 0;
  virtual std::u16string GetOkButtonLabel() const = 0;
  virtual int GetCvcImageRid() const = 0;
  virtual bool ShouldRequestExpirationDate() const = 0;
  virtual bool GetStoreLocallyStartState() const = 0;
#if defined(OS_ANDROID)
  virtual int GetGooglePayImageRid() const = 0;
  virtual bool ShouldOfferWebauthn() const = 0;
  virtual bool GetWebauthnOfferStartState() const = 0;
  virtual bool IsCardLocal() const = 0;
#endif
  virtual base::TimeDelta GetSuccessMessageDuration() const = 0;
  virtual AutofillClient::PaymentsRpcResult GetVerificationResult() const = 0;

  // Utilities.
  virtual bool InputCvcIsValid(const std::u16string& input_text) const = 0;
  virtual bool InputExpirationIsValid(const std::u16string& month,
                                      const std::u16string& year) const = 0;
  virtual int GetExpectedCvcLength() const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_UNMASK_PROMPT_CONTROLLER_H_
