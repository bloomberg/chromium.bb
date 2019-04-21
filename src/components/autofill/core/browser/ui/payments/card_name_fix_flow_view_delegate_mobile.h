// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_VIEW_DELEGATE_MOBILE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_VIEW_DELEGATE_MOBILE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_metrics.h"

namespace autofill {

// Enables the user to accept or deny cardholder name fix flow prompt.
// Only used on mobile.
class CardNameFixFlowViewDelegateMobile {
 public:
  CardNameFixFlowViewDelegateMobile(
      const base::string16& inferred_cardholder_name,
      base::OnceCallback<void(const base::string16&)>
          upload_save_card_callback);

  ~CardNameFixFlowViewDelegateMobile();

  int GetIconId() const;
  base::string16 GetTitleText() const;
  base::string16 GetInferredCardHolderName() const;
  base::string16 GetSaveButtonLabel() const;
  void Accept(const base::string16& name);
  void Dismissed();
  void Shown();

 private:
  // Inferred cardholder name from Gaia account.
  base::string16 inferred_cardholder_name_;

  // The callback to save the credit card to Google Payments once user accepts
  // fix flow.
  base::OnceCallback<void(const base::string16&)> upload_save_card_callback_;

  // Whether the prompt was shown to the user.
  bool shown_;

  // Did the user ever explicitly accept or dismiss this prompt?
  bool had_user_interaction_;

  DISALLOW_COPY_AND_ASSIGN(CardNameFixFlowViewDelegateMobile);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_VIEW_DELEGATE_MOBILE_H_
