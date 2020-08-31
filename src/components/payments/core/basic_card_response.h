// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_BASIC_CARD_RESPONSE_H_
#define COMPONENTS_PAYMENTS_CORE_BASIC_CARD_RESPONSE_H_

#include <memory>

#include "base/strings/string16.h"
#include "components/payments/core/payment_address.h"

namespace base {
class DictionaryValue;
}

namespace payments {

// Contains the response from the PaymentRequest API when a user accepts
// payment with a Basic Payment Card payment method.
struct BasicCardResponse {
 public:
  BasicCardResponse();
  ~BasicCardResponse();

  bool operator==(const BasicCardResponse& other) const;
  bool operator!=(const BasicCardResponse& other) const;

  // Populates |value| with the properties of this BasicCardResponse.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue() const;

  // The cardholder's name as it appears on the card.
  base::string16 cardholder_name;

  // The primary account number (PAN) for the payment card.
  base::string16 card_number;

  // A two-digit string for the expiry month of the card in the range 01 to 12.
  base::string16 expiry_month;

  // A two-digit string for the expiry year of the card in the range 00 to 99.
  base::string16 expiry_year;

  // A three or four digit string for the security code of the card (sometimes
  // known as the CVV, CVC, CVN, CVE or CID).
  base::string16 card_security_code;

  // The billing address information associated with the payment card.
  mojom::PaymentAddressPtr billing_address;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_BASIC_CARD_RESPONSE_H_
