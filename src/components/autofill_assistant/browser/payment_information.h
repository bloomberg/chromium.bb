// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PAYMENT_INFORMATION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PAYMENT_INFORMATION_H_

#include <string>

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace autofill_assistant {

// Struct for holding the payment information data.
struct PaymentInformation {
  PaymentInformation();
  ~PaymentInformation();

  bool succeed;
  std::unique_ptr<autofill::CreditCard> card;
  std::unique_ptr<autofill::AutofillProfile> shipping_address;
  std::unique_ptr<autofill::AutofillProfile> billing_address;
  std::string payer_name;
  std::string payer_phone;
  std::string payer_email;
  bool is_terms_and_conditions_accepted;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PAYMENT_INFORMATION_H_
