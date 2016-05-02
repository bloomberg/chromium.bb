// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentDetailsTestHelper.h"

#include "modules/payments/CurrencyAmount.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {
namespace {

// PaymentItem and ShippingOption have identical structure.
template <typename PaymentItemOrShippingOption>
void setValues(PaymentItemOrShippingOption& original, PaymentTestDataToChange data, PaymentTestModificationType modificationType, const String& valueToUse)
{
    CurrencyAmount itemAmount;
    if (data == PaymentTestDataCurrencyCode) {
        if (modificationType == PaymentTestOverwriteValue)
            itemAmount.setCurrencyCode(valueToUse);
    } else {
        itemAmount.setCurrencyCode("USD");
    }
    if (data == PaymentTestDataValue) {
        if (modificationType == PaymentTestOverwriteValue)
            itemAmount.setValue(valueToUse);
    } else {
        itemAmount.setValue("9.99");
    }

    if (data != PaymentTestDataAmount || modificationType != PaymentTestRemoveKey)
        original.setAmount(itemAmount);

    if (data == PaymentTestDataId) {
        if (modificationType == PaymentTestOverwriteValue)
            original.setId(valueToUse);
    } else {
        original.setId("id");
    }
    if (data == PaymentTestDataLabel) {
        if (modificationType == PaymentTestOverwriteValue)
            original.setLabel(valueToUse);
    } else {
        original.setLabel("Label");
    }
}

} // namespace

PaymentItem buildPaymentItemForTest(PaymentTestDataToChange data, PaymentTestModificationType modificationType, const String& valueToUse)
{
    PaymentItem item;
    setValues(item, data, modificationType, valueToUse);
    return item;
}

ShippingOption buildShippingOptionForTest(PaymentTestDataToChange data, PaymentTestModificationType modificationType, const String& valueToUse)
{
    ShippingOption shippingOption;
    setValues(shippingOption, data, modificationType, valueToUse);
    return shippingOption;
}

PaymentDetails buildPaymentDetailsForTest(PaymentTestDetailToChange detail, PaymentTestDataToChange data, PaymentTestModificationType modificationType, const String& valueToUse)
{
    PaymentItem item;
    if (detail == PaymentTestDetailItem)
        item = buildPaymentItemForTest(data, modificationType, valueToUse);
    else
        item = buildPaymentItemForTest();

    ShippingOption shippingOption;
    if (detail == PaymentTestDetailShippingOption)
        shippingOption = buildShippingOptionForTest(data, modificationType, valueToUse);
    else
        shippingOption = buildShippingOptionForTest();

    PaymentDetails result;
    result.setItems(HeapVector<PaymentItem>(1, item));
    result.setShippingOptions(HeapVector<ShippingOption>(2, shippingOption));

    return result;
}

} // namespace blink
