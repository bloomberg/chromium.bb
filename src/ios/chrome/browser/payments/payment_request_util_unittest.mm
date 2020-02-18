// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/payment_request_util.h"

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_response.h"
#include "components/payments/mojom/payment_request_data.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace payment_request_util {

using PaymentRequestUtilTest = PlatformTest;

// Tests that serializing a default PaymentResponse yields the expected result.
TEST_F(PaymentRequestUtilTest,
       PaymentResponseToDictionaryValue_EmptyResponseDictionary) {
  base::Value expected_value(base::Value::Type::DICTIONARY);

  expected_value.SetKey("requestId", base::Value(base::Value::Type::STRING));
  expected_value.SetKey("methodName", base::Value(base::Value::Type::STRING));
  expected_value.SetKey("details", base::Value());
  expected_value.SetKey("shippingAddress", base::Value());
  expected_value.SetKey("shippingOption",
                        base::Value(base::Value::Type::STRING));
  expected_value.SetKey("payerName", base::Value(base::Value::Type::STRING));
  expected_value.SetKey("payerEmail", base::Value(base::Value::Type::STRING));
  expected_value.SetKey("payerPhone", base::Value(base::Value::Type::STRING));

  payments::PaymentResponse payment_response;
  EXPECT_EQ(expected_value, PaymentResponseToValue(payment_response));
}

// Tests that serializing a populated PaymentResponse yields the expected
// result.
TEST_F(PaymentRequestUtilTest,
       PaymentResponseToDictionaryValue_PopulatedResponseDictionary) {
  base::Value expected_value(base::Value::Type::DICTIONARY);

  base::Value details(base::Value::Type::DICTIONARY);
  details.SetKey("cardNumber", base::Value("1111-1111-1111-1111"));
  details.SetKey("cardholderName", base::Value("Jon Doe"));
  details.SetKey("expiryMonth", base::Value("02"));
  details.SetKey("expiryYear", base::Value("2090"));
  details.SetKey("cardSecurityCode", base::Value("111"));
  base::Value billing_address(base::Value::Type::DICTIONARY);
  billing_address.SetKey("country", base::Value(base::Value::Type::STRING));
  billing_address.SetKey("addressLine", base::Value(base::Value::Type::LIST));
  billing_address.SetKey("region", base::Value(base::Value::Type::STRING));
  billing_address.SetKey("dependentLocality",
                         base::Value(base::Value::Type::STRING));
  billing_address.SetKey("city", base::Value(base::Value::Type::STRING));
  billing_address.SetKey("postalCode", base::Value("90210"));
  billing_address.SetKey("sortingCode", base::Value(base::Value::Type::STRING));
  billing_address.SetKey("organization",
                         base::Value(base::Value::Type::STRING));
  billing_address.SetKey("recipient", base::Value(base::Value::Type::STRING));
  billing_address.SetKey("phone", base::Value(base::Value::Type::STRING));
  details.SetKey("billingAddress", std::move(billing_address));
  expected_value.SetKey("details", std::move(details));
  expected_value.SetKey("requestId", base::Value("12345"));
  expected_value.SetKey("methodName", base::Value("American Express"));
  base::Value shipping_address(base::Value::Type::DICTIONARY);
  shipping_address.SetKey("country", base::Value(base::Value::Type::STRING));
  shipping_address.SetKey("addressLine", base::Value(base::Value::Type::LIST));
  shipping_address.SetKey("region", base::Value(base::Value::Type::STRING));
  shipping_address.SetKey("dependentLocality",
                          base::Value(base::Value::Type::STRING));
  shipping_address.SetKey("city", base::Value(base::Value::Type::STRING));
  shipping_address.SetKey("postalCode", base::Value("94115"));
  shipping_address.SetKey("sortingCode",
                          base::Value(base::Value::Type::STRING));
  shipping_address.SetKey("organization",
                          base::Value(base::Value::Type::STRING));
  shipping_address.SetKey("recipient", base::Value(base::Value::Type::STRING));
  shipping_address.SetKey("phone", base::Value(base::Value::Type::STRING));
  expected_value.SetKey("shippingAddress", std::move(shipping_address));
  expected_value.SetKey("shippingOption", base::Value("666"));
  expected_value.SetKey("payerName", base::Value("Jane Doe"));
  expected_value.SetKey("payerEmail", base::Value("jane@example.com"));
  expected_value.SetKey("payerPhone", base::Value("1234-567-890"));

  payments::PaymentResponse payment_response;
  payment_response.payment_request_id = "12345";
  payment_response.method_name = "American Express";

  payments::BasicCardResponse payment_response_details;
  payment_response_details.card_number =
      base::ASCIIToUTF16("1111-1111-1111-1111");
  payment_response_details.cardholder_name = base::ASCIIToUTF16("Jon Doe");
  payment_response_details.expiry_month = base::ASCIIToUTF16("02");
  payment_response_details.expiry_year = base::ASCIIToUTF16("2090");
  payment_response_details.card_security_code = base::ASCIIToUTF16("111");
  payment_response_details.billing_address->postal_code = "90210";
  std::unique_ptr<base::DictionaryValue> response_value =
      payment_response_details.ToDictionaryValue();
  std::string payment_response_stringified_details;
  base::JSONWriter::Write(*response_value,
                          &payment_response_stringified_details);
  payment_response.details = payment_response_stringified_details;

  payment_response.shipping_address = payments::mojom::PaymentAddress::New();
  payment_response.shipping_address->postal_code = "94115";
  payment_response.shipping_option = "666";
  payment_response.payer_name = base::ASCIIToUTF16("Jane Doe");
  payment_response.payer_email = base::ASCIIToUTF16("jane@example.com");
  payment_response.payer_phone = base::ASCIIToUTF16("1234-567-890");
  EXPECT_EQ(expected_value, PaymentResponseToValue(payment_response));
}

}  // namespace payment_request_util
