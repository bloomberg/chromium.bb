// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentDetailsTestHelper.h"

#include "modules/payments/CurrencyAmount.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

PaymentItem buildPaymentItemForTest(PaymentTestDataToChange data, PaymentTestModificationType modificationType, const String& valueToUse)
{
    CurrencyAmount itemAmount;
    if (data == PaymentTestDataCurrencyCode) {
        if (modificationType == PaymentTestOverwriteValue)
            itemAmount.setCurrencyCode(valueToUse);
    } else {
        itemAmount.setCurrencyCode("USD");
    }
    if (data == PaymentTestDataAmount) {
        if (modificationType == PaymentTestOverwriteValue)
            itemAmount.setValue(valueToUse);
    } else {
        itemAmount.setValue("9.99");
    }

    PaymentItem item;
    item.setAmount(itemAmount);
    if (data == PaymentTestDataId) {
        if (modificationType == PaymentTestOverwriteValue)
            item.setId(valueToUse);
    } else {
        item.setId("total");
    }
    if (data == PaymentTestDataLabel) {
        if (modificationType == PaymentTestOverwriteValue)
            item.setLabel(valueToUse);
    } else {
        item.setLabel("Total charge");
    }

    return item;
}

ShippingOption buildShippingOptionForTest(PaymentTestDataToChange data, PaymentTestModificationType modificationType, const String& valueToUse)
{
    CurrencyAmount shippingAmount;
    if (data == PaymentTestDataCurrencyCode) {
        if (modificationType == PaymentTestOverwriteValue)
            shippingAmount.setCurrencyCode(valueToUse);
    } else {
        shippingAmount.setCurrencyCode("USD");
    }
    if (data == PaymentTestDataAmount) {
        if (modificationType == PaymentTestOverwriteValue)
            shippingAmount.setValue(valueToUse);
    } else {
        shippingAmount.setValue("9.99");
    }

    ShippingOption shippingOption;
    shippingOption.setAmount(shippingAmount);
    if (data == PaymentTestDataId) {
        if (modificationType == PaymentTestOverwriteValue)
            shippingOption.setId(valueToUse);
    } else {
        shippingOption.setId("standard");
    }
    if (data == PaymentTestDataLabel) {
        if (modificationType == PaymentTestOverwriteValue)
            shippingOption.setLabel(valueToUse);
    } else {
        shippingOption.setLabel("Standard shipping");
    }

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
