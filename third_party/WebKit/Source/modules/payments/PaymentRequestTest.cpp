// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequest.h"

#include "bindings/core/v8/JSONValuesForV8.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/modules/v8/V8PaymentResponse.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/payments/PaymentAddress.h"
#include "modules/payments/PaymentRequestTestBase.h"
#include "modules/payments/PaymentResponse.h"
#include "modules/payments/PaymentTestHelper.h"
#include "platform/heap/HeapAllocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <utility>

namespace blink {
namespace {

class PaymentRequestTest : public PaymentRequestTestBase {};

TEST_F(PaymentRequestTest, NoExceptionWithValidData)
{
    PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());

    EXPECT_FALSE(getExceptionState().hadException());
}

TEST_F(PaymentRequestTest, SecureContextRequired)
{
    setSecurityOrigin("http://www.example.com");

    PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(SecurityError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, SupportedMethodListRequired)
{
    PaymentRequest::create(getScriptState(), HeapVector<PaymentMethodData>(), buildPaymentDetailsForTest(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, TotalRequired)
{
    PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), PaymentDetails(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, NullShippingOptionWhenNoOptionsAvailable)
{
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), details, options, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, NullShippingOptionWhenMultipleOptionsAvailable)
{
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    details.setShippingOptions(HeapVector<ShippingOption>(2, buildShippingOptionForTest()));
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), details, options, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, DontSelectSingleAvailableShippingOptionByDefault)
{
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    details.setShippingOptions(HeapVector<ShippingOption>(1, buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "standard")));

    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), details, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, DontSelectSingleAvailableShippingOptionWhenShippingNotRequested)
{
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    details.setShippingOptions(HeapVector<ShippingOption>(1, buildShippingOptionForTest()));
    PaymentOptions options;
    options.setRequestShipping(false);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), details, options, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, DontSelectSingleUnselectedShippingOptionWhenShippingRequested)
{
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    details.setShippingOptions(HeapVector<ShippingOption>(1, buildShippingOptionForTest()));
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), details, options, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, SelectSingleSelectedShippingOptionWhenShippingRequested)
{
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    HeapVector<ShippingOption> shippingOptions(1, buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "standard"));
    shippingOptions[0].setSelected(true);
    details.setShippingOptions(shippingOptions);
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), details, options, getExceptionState());

    EXPECT_EQ("standard", request->shippingOption());
}

TEST_F(PaymentRequestTest, SelectOnlySelectedShippingOptionWhenShippingRequested)
{
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    HeapVector<ShippingOption> shippingOptions(2);
    shippingOptions[0] = buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "standard");
    shippingOptions[0].setSelected(true);
    shippingOptions[1] = buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "express");
    details.setShippingOptions(shippingOptions);
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), details, options, getExceptionState());

    EXPECT_EQ("standard", request->shippingOption());
}

TEST_F(PaymentRequestTest, SelectLastSelectedShippingOptionWhenShippingRequested)
{
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    HeapVector<ShippingOption> shippingOptions(2);
    shippingOptions[0] = buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "standard");
    shippingOptions[0].setSelected(true);
    shippingOptions[1] = buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "express");
    shippingOptions[1].setSelected(true);
    details.setShippingOptions(shippingOptions);
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), details, options, getExceptionState());

    EXPECT_EQ("express", request->shippingOption());
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnInvalidShippingAddress)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(expectNoCall(), expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnShippingAddressChange(mojom::blink::PaymentAddress::New());
}

TEST_F(PaymentRequestTest, RejectShowPromiseWithRequestShippingTrueAndEmptyShippingAddressInResponse)
{
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = buildPaymentResponseForTest();

    request->show(getScriptState()).then(expectNoCall(), expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

TEST_F(PaymentRequestTest, RejectShowPromiseWithRequestShippingTrueAndInvalidShippingAddressInResponse)
{
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = buildPaymentResponseForTest();
    response->shipping_address = mojom::blink::PaymentAddress::New();

    request->show(getScriptState()).then(expectNoCall(), expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

TEST_F(PaymentRequestTest, RejectShowPromiseWithRequestShippingFalseAndShippingAddressExistsInResponse)
{
    PaymentOptions options;
    options.setRequestShipping(false);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->shipping_address = mojom::blink::PaymentAddress::New();
    response->shipping_address->country = "US";
    response->shipping_address->language_code = "en";
    response->shipping_address->script_code = "Latn";

    request->show(getScriptState()).then(expectNoCall(), expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

class PaymentResponseFunction : public ScriptFunction {
public:
    static v8::Local<v8::Function> create(ScriptState* scriptState, ScriptValue* outValue)
    {
        PaymentResponseFunction* self = new PaymentResponseFunction(scriptState, outValue);
        return self->bindToV8Function();
    }

    ScriptValue call(ScriptValue value) override
    {
        DCHECK(!value.isEmpty());
        *m_value = value;
        return value;
    }

private:
    PaymentResponseFunction(ScriptState* scriptState, ScriptValue* outValue)
        : ScriptFunction(scriptState)
        , m_value(outValue)
    {
        DCHECK(m_value);
    }

    ScriptValue* m_value;
};

TEST_F(PaymentRequestTest, ResolveShowPromiseWithRequestShippingTrueAndValidShippingAddressInResponse)
{
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = buildPaymentResponseForTest();
    response->shipping_address = mojom::blink::PaymentAddress::New();
    response->shipping_address->country = "US";
    response->shipping_address->language_code = "en";
    response->shipping_address->script_code = "Latn";

    ScriptValue outValue;
    request->show(getScriptState()).then(PaymentResponseFunction::create(getScriptState(), &outValue), expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
    firePromiseCallbacks();
    PaymentResponse* pr = V8PaymentResponse::toImplWithTypeCheck(getScriptState()->isolate(), outValue.v8Value());

    EXPECT_EQ("US", pr->shippingAddress()->country());
    EXPECT_EQ("en-Latn", pr->shippingAddress()->languageCode());
}

TEST_F(PaymentRequestTest, ResolveShowPromiseWithRequestShippingFalseAndEmptyShippingAddressInResponse)
{
    PaymentOptions options;
    options.setRequestShipping(false);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    ScriptValue outValue;
    request->show(getScriptState()).then(PaymentResponseFunction::create(getScriptState(), &outValue), expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());
    firePromiseCallbacks();
    PaymentResponse* pr = V8PaymentResponse::toImplWithTypeCheck(getScriptState()->isolate(), outValue.v8Value());

    EXPECT_EQ(nullptr, pr->shippingAddress());
}

TEST_F(PaymentRequestTest, OnShippingOptionChange)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(expectNoCall(), expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnShippingOptionChange("standardShipping");
}

TEST_F(PaymentRequestTest, CannotCallShowTwice)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState());

    request->show(getScriptState()).then(expectNoCall(), expectCall());
}

TEST_F(PaymentRequestTest, CannotCallCompleteTwice)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());
    request->complete(getScriptState(), false);

    request->complete(getScriptState(), true).then(expectNoCall(), expectCall());
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnError)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(expectNoCall(), expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnError();
}

TEST_F(PaymentRequestTest, RejectCompletePromiseOnError)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());

    request->complete(getScriptState(), true).then(expectNoCall(), expectCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnError();
}

TEST_F(PaymentRequestTest, ResolvePromiseOnComplete)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());

    request->complete(getScriptState(), true).then(expectCall(), expectNoCall());

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnComplete();
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnUpdateDetailsFailure)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(expectNoCall(), expectCall());

    request->onUpdatePaymentDetailsFailure(ScriptValue::from(getScriptState(), "oops"));
}

TEST_F(PaymentRequestTest, RejectCompletePromiseOnUpdateDetailsFailure)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState()).then(expectCall(), expectNoCall());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());

    request->complete(getScriptState(), true).then(expectNoCall(), expectCall());

    request->onUpdatePaymentDetailsFailure(ScriptValue::from(getScriptState(), "oops"));
}

TEST_F(PaymentRequestTest, IgnoreUpdatePaymentDetailsAfterShowPromiseResolved)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState()).then(expectCall(), expectNoCall());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(buildPaymentResponseForTest());

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), "foo"));
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnNonPaymentDetailsUpdate)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(expectNoCall(), expectCall());

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), "NotPaymentDetails"));
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnInvalidPaymentDetailsUpdate)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(expectNoCall(), expectCall());

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), fromJSONString(getScriptState(), "{}", getExceptionState())));
    EXPECT_FALSE(getExceptionState().hadException());
}

TEST_F(PaymentRequestTest, ClearShippingOptionOnPaymentDetailsUpdateWithoutShippingOptions)
{
    PaymentDetails details;
    details.setTotal(buildPaymentItemForTest());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), details, options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    EXPECT_TRUE(request->shippingOption().isNull());
    request->show(getScriptState()).then(expectNoCall(), expectNoCall());
    String detailWithShippingOptions = "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
        "\"shippingOptions\": [{\"id\": \"standardShippingOption\", \"label\": \"Standard shipping\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}, \"selected\": true}]}";
    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), fromJSONString(getScriptState(), detailWithShippingOptions, getExceptionState())));
    EXPECT_FALSE(getExceptionState().hadException());
    EXPECT_EQ("standardShippingOption", request->shippingOption());
    String detailWithoutShippingOptions = "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}}}";

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), fromJSONString(getScriptState(), detailWithoutShippingOptions, getExceptionState())));

    EXPECT_FALSE(getExceptionState().hadException());
    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, ClearShippingOptionOnPaymentDetailsUpdateWithMultipleUnselectedShippingOptions)
{
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState()).then(expectNoCall(), expectNoCall());
    String detail = "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
        "\"shippingOptions\": [{\"id\": \"slow\", \"label\": \"Slow\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
        "{\"id\": \"fast\", \"label\": \"Fast\", \"amount\": {\"currency\": \"USD\", \"value\": \"50.00\"}}]}";

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), fromJSONString(getScriptState(), detail, getExceptionState())));
    EXPECT_FALSE(getExceptionState().hadException());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, UseTheSelectedShippingOptionFromPaymentDetailsUpdate)
{
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), buildPaymentMethodDataForTest(), buildPaymentDetailsForTest(), options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState()).then(expectNoCall(), expectNoCall());
    String detail = "{\"total\": {\"label\": \"Total\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
        "\"shippingOptions\": [{\"id\": \"slow\", \"label\": \"Slow\", \"amount\": {\"currency\": \"USD\", \"value\": \"5.00\"}},"
        "{\"id\": \"fast\", \"label\": \"Fast\", \"amount\": {\"currency\": \"USD\", \"value\": \"50.00\"}, \"selected\": true}]}";

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), fromJSONString(getScriptState(), detail, getExceptionState())));
    EXPECT_FALSE(getExceptionState().hadException());

    EXPECT_EQ("fast", request->shippingOption());
}

} // namespace
} // namespace blink
