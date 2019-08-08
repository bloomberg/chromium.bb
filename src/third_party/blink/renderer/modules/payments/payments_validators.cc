// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payments_validators.h"

#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/modules/payments/address_errors.h"
#include "third_party/blink/renderer/modules/payments/payer_errors.h"
#include "third_party/blink/renderer/modules/payments/payment_validation_errors.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
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

bool PaymentsValidators::IsValidShippingAddress(
    const payments::mojom::blink::PaymentAddressPtr& address,
    String* optional_error_message) {
  return IsValidCountryCodeFormat(address->country, optional_error_message);
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
bool PaymentsValidators::IsValidAddressErrorsFormat(
    const AddressErrors* errors,
    String* optional_error_message) {
  return (!errors->hasAddressLine() ||
          IsValidErrorMsgFormat(errors->addressLine(),
                                optional_error_message)) &&
         (!errors->hasCity() ||
          IsValidErrorMsgFormat(errors->city(), optional_error_message)) &&
         (!errors->hasCountry() ||
          IsValidErrorMsgFormat(errors->country(), optional_error_message)) &&
         (!errors->hasDependentLocality() ||
          IsValidErrorMsgFormat(errors->dependentLocality(),
                                optional_error_message)) &&
         (!errors->hasOrganization() ||
          IsValidErrorMsgFormat(errors->organization(),
                                optional_error_message)) &&
         (!errors->hasPhone() ||
          IsValidErrorMsgFormat(errors->phone(), optional_error_message)) &&
         (!errors->hasPostalCode() ||
          IsValidErrorMsgFormat(errors->postalCode(),
                                optional_error_message)) &&
         (!errors->hasRecipient() ||
          IsValidErrorMsgFormat(errors->recipient(), optional_error_message)) &&
         (!errors->hasRegion() ||
          IsValidErrorMsgFormat(errors->region(), optional_error_message)) &&
         (!errors->hasSortingCode() ||
          IsValidErrorMsgFormat(errors->sortingCode(), optional_error_message));
}

// static
bool PaymentsValidators::IsValidPayerErrorsFormat(
    const PayerErrors* errors,
    String* optional_error_message) {
  return (!errors->hasEmail() ||
          IsValidErrorMsgFormat(errors->email(), optional_error_message)) &&
         (!errors->hasName() ||
          IsValidErrorMsgFormat(errors->name(), optional_error_message)) &&
         (!errors->hasPhone() ||
          IsValidErrorMsgFormat(errors->phone(), optional_error_message));
}

// static
bool PaymentsValidators::IsValidPaymentValidationErrorsFormat(
    const PaymentValidationErrors* errors,
    String* optional_error_message) {
  return (!errors->hasError() ||
          IsValidErrorMsgFormat(errors->error(), optional_error_message)) &&
         (!errors->hasPayer() ||
          IsValidPayerErrorsFormat(errors->payer(), optional_error_message)) &&
         (!errors->hasShippingAddress() ||
          IsValidAddressErrorsFormat(errors->shippingAddress(),
                                     optional_error_message));
}

bool PaymentsValidators::IsValidMethodFormat(const String& identifier) {
  KURL url(NullURL(), identifier);
  if (url.IsValid()) {
    // Allow localhost payment method for test.
    if (SecurityOrigin::Create(url)->IsLocalhost())
      return true;

    // URL PMI validation rules:
    // https://www.w3.org/TR/payment-method-id/#dfn-validate-a-url-based-payment-method-identifier
    return url.Protocol() == "https" && url.User().IsEmpty() &&
           url.Pass().IsEmpty();
  } else {
    // Syntax for a valid standardized PMI:
    // https://www.w3.org/TR/payment-method-id/#dfn-syntax-of-a-standardized-payment-method-identifier
    return ScriptRegexp("^[a-z]+[0-9a-z]*(-[a-z]+[0-9a-z]*)*$",
                        kTextCaseSensitive)
               .Match(identifier) == 0;
  }
}

}  // namespace blink
