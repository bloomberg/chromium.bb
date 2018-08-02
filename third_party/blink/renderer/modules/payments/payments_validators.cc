// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payments_validators.h"

#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/modules/payments/payment_validation_errors.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"

namespace blink {

// We limit the maximum length of string to 2048 bytes for security reasons.
static const int kMaxiumStringLength = 2048;

bool PaymentsValidators::IsValidCurrencyCodeFormat(
    const String& code,
    String* optional_error_message) {
  if (ScriptRegexp("^[A-Z]{3}$", kTextCaseUnicodeInsensitive).Match(code) == 0)
    return true;

  if (optional_error_message) {
    *optional_error_message = "'" + code +
                              "' is not a valid ISO 4217 currency code, should "
                              "be well-formed 3-letter alphabetic code.";
  }

  return false;
}

bool PaymentsValidators::IsValidAmountFormat(const String& amount,
                                             const String& item_name,
                                             String* optional_error_message) {
  if (ScriptRegexp("^-?[0-9]+(\\.[0-9]+)?$", kTextCaseSensitive)
          .Match(amount) == 0)
    return true;

  if (optional_error_message) {
    *optional_error_message =
        "'" + amount + "' is not a valid amount format for " + item_name;
  }

  return false;
}

bool PaymentsValidators::IsValidCountryCodeFormat(
    const String& code,
    String* optional_error_message) {
  if (ScriptRegexp("^[A-Z]{2}$", kTextCaseSensitive).Match(code) == 0)
    return true;

  if (optional_error_message)
    *optional_error_message = "'" + code +
                              "' is not a valid CLDR country code, should be 2 "
                              "upper case letters [A-Z]";

  return false;
}

bool PaymentsValidators::IsValidLanguageCodeFormat(
    const String& code,
    String* optional_error_message) {
  if (ScriptRegexp("^([a-z]{2,3})?$", kTextCaseSensitive).Match(code) == 0)
    return true;

  if (optional_error_message)
    *optional_error_message =
        "'" + code +
        "' is not a valid BCP-47 language code, should be "
        "2-3 lower case letters [a-z]";

  return false;
}

bool PaymentsValidators::IsValidScriptCodeFormat(
    const String& code,
    String* optional_error_message) {
  if (ScriptRegexp("^([A-Z][a-z]{3})?$", kTextCaseSensitive).Match(code) == 0)
    return true;

  if (optional_error_message)
    *optional_error_message =
        "'" + code +
        "' is not a valid ISO 15924 script code, should be "
        "an upper case letter [A-Z] followed by 3 lower "
        "case letters [a-z]";

  return false;
}

bool PaymentsValidators::IsValidShippingAddress(
    const payments::mojom::blink::PaymentAddressPtr& address,
    String* optional_error_message) {
  if (!IsValidCountryCodeFormat(address->country, optional_error_message))
    return false;

  if (!IsValidLanguageCodeFormat(address->language_code,
                                 optional_error_message))
    return false;

  if (!IsValidScriptCodeFormat(address->script_code, optional_error_message))
    return false;

  if (address->language_code.IsEmpty() && !address->script_code.IsEmpty()) {
    if (optional_error_message)
      *optional_error_message =
          "If language code is empty, then script code should also be empty";

    return false;
  }

  return true;
}

bool PaymentsValidators::IsValidErrorMsgFormat(const String& error,
                                               String* optional_error_message) {
  if (error.length() <= kMaxiumStringLength)
    return true;

  if (optional_error_message)
    *optional_error_message =
        "Error message should be at most 2048 characters long";

  return false;
}

// static
bool PaymentsValidators::IsValidPaymentValidationErrorsFormat(
    const PaymentValidationErrors& errors,
    String* optional_error_message) {
  if (errors.hasPayer()) {
    if ((errors.payer().hasEmail() &&
         !IsValidErrorMsgFormat(errors.payer().email(),
                                optional_error_message)) ||
        (errors.payer().hasName() &&
         !IsValidErrorMsgFormat(errors.payer().name(),
                                optional_error_message)) ||
        (errors.payer().hasPhone() &&
         !IsValidErrorMsgFormat(errors.payer().phone(),
                                optional_error_message))) {
      return false;
    }
  }

  if (errors.hasShippingAddress()) {
    if ((errors.shippingAddress().hasAddressLine() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().addressLine(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasCity() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().city(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasCountry() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().country(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasDependentLocality() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().dependentLocality(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasLanguageCode() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().languageCode(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasOrganization() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().organization(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasPhone() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().phone(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasPostalCode() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().postalCode(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasRecipient() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().recipient(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasRegion() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().region(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasRegionCode() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().regionCode(),
                                optional_error_message)) ||
        (errors.shippingAddress().hasSortingCode() &&
         !IsValidErrorMsgFormat(errors.shippingAddress().sortingCode(),
                                optional_error_message))) {
      return false;
    }
  }

  return true;
}

}  // namespace blink
