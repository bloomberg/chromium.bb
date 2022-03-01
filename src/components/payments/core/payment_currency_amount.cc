// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_currency_amount.h"

#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/browser-payment-api/#dom-paymentcurrencyamount
static const char kPaymentCurrencyAmountCurrency[] = "currency";
static const char kPaymentCurrencyAmountValue[] = "value";

}  // namespace

bool PaymentCurrencyAmountFromValue(const base::Value& dictionary_value,
                                    mojom::PaymentCurrencyAmount* amount) {
  if (!dictionary_value.is_dict())
    return false;

  const std::string* currency =
      dictionary_value.FindStringKey(kPaymentCurrencyAmountCurrency);
  if (!currency) {
    return false;
  }
  amount->currency = *currency;

  const std::string* value =
      dictionary_value.FindStringKey(kPaymentCurrencyAmountValue);
  if (!value) {
    return false;
  }
  amount->value = *value;

  return true;
}

base::Value PaymentCurrencyAmountToValue(
    const mojom::PaymentCurrencyAmount& amount) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetStringKey(kPaymentCurrencyAmountCurrency, amount.currency);
  result.SetStringKey(kPaymentCurrencyAmountValue, amount.value);

  return result;
}

}  // namespace payments
