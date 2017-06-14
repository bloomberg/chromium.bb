// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentTestHelper_h
#define PaymentTestHelper_h

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/V8DOMException.h"
#include "modules/payments/PaymentDetailsInit.h"
#include "modules/payments/PaymentDetailsUpdate.h"
#include "modules/payments/PaymentItem.h"
#include "modules/payments/PaymentShippingOption.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/Persistent.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/modules/payments/payment_request.mojom-blink.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

class Document;
class PaymentMethodData;
class ScriptState;
class ScriptValue;

enum PaymentTestDetailToChange {
  kPaymentTestDetailNone,
  kPaymentTestDetailTotal,
  kPaymentTestDetailItem,
  kPaymentTestDetailShippingOption,
  kPaymentTestDetailModifierTotal,
  kPaymentTestDetailModifierItem,
  kPaymentTestDetailError
};

enum PaymentTestDataToChange {
  kPaymentTestDataNone,
  kPaymentTestDataId,
  kPaymentTestDataLabel,
  kPaymentTestDataAmount,
  kPaymentTestDataCurrencyCode,
  kPaymentTestDataCurrencySystem,
  kPaymentTestDataValue,
};

enum PaymentTestModificationType {
  kPaymentTestOverwriteValue,
  kPaymentTestRemoveKey
};

PaymentItem BuildPaymentItemForTest(
    PaymentTestDataToChange = kPaymentTestDataNone,
    PaymentTestModificationType = kPaymentTestOverwriteValue,
    const String& value_to_use = String());

PaymentShippingOption BuildShippingOptionForTest(
    PaymentTestDataToChange = kPaymentTestDataNone,
    PaymentTestModificationType = kPaymentTestOverwriteValue,
    const String& value_to_use = String());

PaymentDetailsModifier BuildPaymentDetailsModifierForTest(
    PaymentTestDetailToChange = kPaymentTestDetailNone,
    PaymentTestDataToChange = kPaymentTestDataNone,
    PaymentTestModificationType = kPaymentTestOverwriteValue,
    const String& value_to_use = String());

PaymentDetailsInit BuildPaymentDetailsInitForTest(
    PaymentTestDetailToChange = kPaymentTestDetailNone,
    PaymentTestDataToChange = kPaymentTestDataNone,
    PaymentTestModificationType = kPaymentTestOverwriteValue,
    const String& value_to_use = String());

PaymentDetailsUpdate BuildPaymentDetailsUpdateForTest(
    PaymentTestDetailToChange = kPaymentTestDetailNone,
    PaymentTestDataToChange = kPaymentTestDataNone,
    PaymentTestModificationType = kPaymentTestOverwriteValue,
    const String& value_to_use = String());

PaymentDetailsUpdate BuildPaymentDetailsErrorMsgForTest(
    const String& value_to_use = String());

HeapVector<PaymentMethodData> BuildPaymentMethodDataForTest();

payments::mojom::blink::PaymentResponsePtr BuildPaymentResponseForTest();

void MakePaymentRequestOriginSecure(Document&);

class PaymentRequestMockFunctionScope {
  STACK_ALLOCATED();

 public:
  explicit PaymentRequestMockFunctionScope(ScriptState*);
  ~PaymentRequestMockFunctionScope();

  v8::Local<v8::Function> ExpectCall();
  v8::Local<v8::Function> ExpectCall(String* captor);
  v8::Local<v8::Function> ExpectNoCall();

 private:
  class MockFunction : public ScriptFunction {
   public:
    explicit MockFunction(ScriptState*);
    MockFunction(ScriptState*, String* captor);
    v8::Local<v8::Function> Bind();
    MOCK_METHOD1(Call, ScriptValue(ScriptValue));
    String* value_;
  };

  ScriptState* script_state_;
  Vector<Persistent<MockFunction>> mock_functions_;
};

}  // namespace blink

#endif  // PaymentTestHelper_h
