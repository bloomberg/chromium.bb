// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentDetailsTestHelper_h
#define PaymentDetailsTestHelper_h

#include "modules/payments/PaymentDetails.h"
#include "wtf/text/WTFString.h"

namespace blink {

enum PaymentTestDetailToChange {
    PaymentTestDetailNone,
    PaymentTestDetailItem,
    PaymentTestDetailShippingOption
};

enum PaymentTestDataToChange {
    PaymentTestDataNone,
    PaymentTestDataId,
    PaymentTestDataLabel,
    PaymentTestDataCurrencyCode,
    PaymentTestDataAmount
};

enum PaymentTestModificationType {
    PaymentTestOverwriteValue,
    PaymentTestRemoveKey
};

PaymentDetails buildPaymentDetailsForTest(PaymentTestDetailToChange = PaymentTestDetailNone, PaymentTestDataToChange = PaymentTestDataNone, PaymentTestModificationType = PaymentTestOverwriteValue, const String& valueToUse = String());

} // namespace blink

#endif // PaymentDetailsTestHelper_h
