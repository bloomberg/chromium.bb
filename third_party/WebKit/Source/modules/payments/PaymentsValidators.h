// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentsValidators_h
#define PaymentsValidators_h

#include "modules/ModulesExport.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/payments/payment_request.mojom-blink.h"

namespace blink {

class MODULES_EXPORT PaymentsValidators final {
  STATIC_ONLY(PaymentsValidators);

 public:
  // The most common identifiers are three-letter alphabetic codes as defined by
  // [ISO4217] (for example, "USD" for US Dollars). |system| is a URL that
  // indicates the currency system that the currency identifier belongs to. By
  // default, the value is urn:iso:std:iso:4217 indicating that currency is
  // defined by [[ISO4217]], however any string of at most 2048 characters is
  // considered valid in other currencySystem. Returns false if currency |code|
  // is too long (greater than 2048).
  static bool IsValidCurrencyCodeFormat(const String& code,
                                        const String& system,
                                        String* optional_error_message);

  // Returns true if |amount| is a valid currency code as defined in ISO 20022
  // CurrencyAnd30Amount.
  static bool IsValidAmountFormat(const String& amount,
                                  const String& item_name,
                                  String* optional_error_message);

  // Returns true if |code| is a valid ISO 3166 country code.
  static bool IsValidCountryCodeFormat(const String& code,
                                       String* optional_error_message);

  // Returns true if |code| is a valid ISO 639 language code.
  static bool IsValidLanguageCodeFormat(const String& code,
                                        String* optional_error_message);

  // Returns true if |code| is a valid ISO 15924 script code.
  static bool IsValidScriptCodeFormat(const String& code,
                                      String* optional_error_message);

  // Returns true if the payment address is valid:
  //  - Has a valid region code
  //  - Has a valid language code, if any.
  //  - Has a valid script code, if any.
  // A script code should be present only if language code is present.
  static bool IsValidShippingAddress(
      const payments::mojom::blink::PaymentAddressPtr&,
      String* optional_error_message);

  // Returns false if |error| is too long (greater than 2048).
  static bool IsValidErrorMsgFormat(const String& code,
                                    String* optional_error_message);
};

}  // namespace blink

#endif  // PaymentsValidators_h
