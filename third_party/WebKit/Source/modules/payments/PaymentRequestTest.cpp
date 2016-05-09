// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequest.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/JSONValuesForV8.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/ExceptionCode.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/payments/CurrencyAmount.h"
#include "modules/payments/PaymentDetailsTestHelper.h"
#include "modules/payments/PaymentItem.h"
#include "modules/payments/ShippingOption.h"
#include "platform/heap/HeapAllocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/OwnPtr.h"
#include <utility>

namespace blink {
namespace {

class PaymentRequestTest : public testing::Test {
public:
    PaymentRequestTest()
        : m_page(DummyPageHolder::create())
    {
        setSecurityOrigin("https://www.example.com/");
    }

    ~PaymentRequestTest() override {}

    ScriptState* getScriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    ExceptionState& getExceptionState() { return m_exceptionState; }

    void setSecurityOrigin(const String& securityOrigin)
    {
        m_page->document().setSecurityOrigin(SecurityOrigin::create(KURL(KURL(), securityOrigin)));
    }

private:
    OwnPtr<DummyPageHolder> m_page;
    TrackExceptionState m_exceptionState;
};

TEST_F(PaymentRequestTest, NoExceptionWithValidData)
{
    PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());

    EXPECT_FALSE(getExceptionState().hadException());
}

TEST_F(PaymentRequestTest, SecureContextRequired)
{
    setSecurityOrigin("http://www.example.com");

    PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(SecurityError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, SupportedMethodListRequired)
{
    PaymentRequest::create(getScriptState(), Vector<String>(), buildPaymentDetailsForTest(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, ItemListRequired)
{
    PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), PaymentDetails(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, ItemListIsNotEmpty)
{
    PaymentDetails details;
    details.setItems(HeapVector<PaymentItem>());

    PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, AtLeastOnePaymentDetailsItemRequired)
{
    PaymentDetails details;
    details.setShippingOptions(HeapVector<ShippingOption>(2, buildShippingOptionForTest()));

    PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
    EXPECT_EQ(V8TypeError, getExceptionState().code());
}

TEST_F(PaymentRequestTest, NullShippingOptionWhenNoOptionsAvailable)
{
    PaymentDetails details;
    details.setItems(HeapVector<PaymentItem>(1, buildPaymentItemForTest()));
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, options, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, NullShippingOptionWhenMultipleOptionsAvailable)
{
    PaymentDetails details;
    details.setItems(HeapVector<PaymentItem>(1, buildPaymentItemForTest()));
    details.setShippingOptions(HeapVector<ShippingOption>(2, buildShippingOptionForTest()));
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, options, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, SelectSingleAvailableShippingOptionWhenShippingRequested)
{
    PaymentDetails details;
    details.setItems(HeapVector<PaymentItem>(1, buildPaymentItemForTest()));
    details.setShippingOptions(HeapVector<ShippingOption>(1, buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "standard")));
    PaymentOptions options;
    options.setRequestShipping(true);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, options, getExceptionState());

    EXPECT_EQ("standard", request->shippingOption());
}

TEST_F(PaymentRequestTest, DontSelectSingleAvailableShippingOptionByDefault)
{
    PaymentDetails details;
    details.setItems(HeapVector<PaymentItem>(1, buildPaymentItemForTest()));
    details.setShippingOptions(HeapVector<ShippingOption>(1, buildShippingOptionForTest(PaymentTestDataId, PaymentTestOverwriteValue, "standard")));

    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, DontSelectSingleAvailableShippingOptionWhenShippingNotRequested)
{
    PaymentDetails details;
    details.setItems(HeapVector<PaymentItem>(1, buildPaymentItemForTest()));
    details.setShippingOptions(HeapVector<ShippingOption>(1, buildShippingOptionForTest()));
    PaymentOptions options;
    options.setRequestShipping(false);

    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, options, getExceptionState());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, AbortWithoutShowShouldThrow)
{
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->abort(getExceptionState());
    EXPECT_TRUE(getExceptionState().hadException());
}

class MockFunction : public ScriptFunction {
public:
    static v8::Local<v8::Function> noExpectations(ScriptState* scriptState)
    {
        MockFunction* self = new MockFunction(scriptState);
        return self->bindToV8Function();
    }

    static v8::Local<v8::Function> expectCall(ScriptState* scriptState)
    {
        MockFunction* self = new MockFunction(scriptState);
        EXPECT_CALL(*self, call(testing::_));
        return self->bindToV8Function();
    }

    static v8::Local<v8::Function> expectNoCall(ScriptState* scriptState)
    {
        MockFunction* self = new MockFunction(scriptState);
        EXPECT_CALL(*self, call(testing::_)).Times(0);
        return self->bindToV8Function();
    }

private:
    explicit MockFunction(ScriptState* scriptState)
        : ScriptFunction(scriptState)
    {
        ON_CALL(*this, call(testing::_)).WillByDefault(testing::ReturnArg<0>());
    }

    MOCK_METHOD1(call, ScriptValue(ScriptValue));
};

TEST_F(PaymentRequestTest, CanAbortAfterShow)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::noExpectations(getScriptState()));
    request->abort(getExceptionState());

    EXPECT_FALSE(getExceptionState().hadException());
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnInvalidShippingAddress)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectCall(getScriptState()));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnShippingAddressChange(mojom::blink::ShippingAddress::New());
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnInvalidShippingAddressInResponse)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->shipping_address = mojom::blink::ShippingAddress::New();

    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectCall(getScriptState()));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

TEST_F(PaymentRequestTest, DontRejectShowPromiseForValidShippingAddress)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    mojom::blink::ShippingAddressPtr shippingAddress = mojom::blink::ShippingAddress::New();
    shippingAddress->region_code = "US";
    shippingAddress->language_code = "en";
    shippingAddress->script_code = "Latn";

    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectNoCall(getScriptState()));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnShippingAddressChange(std::move(shippingAddress));
}

TEST_F(PaymentRequestTest, ResolveShowPromiseWithValidShippingAddressInResponse)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    mojom::blink::PaymentResponsePtr response = mojom::blink::PaymentResponse::New();
    response->shipping_address = mojom::blink::ShippingAddress::New();
    response->shipping_address->region_code = "US";
    response->shipping_address->language_code = "en";
    response->shipping_address->script_code = "Latn";

    request->show(getScriptState()).then(MockFunction::expectCall(getScriptState()), MockFunction::expectNoCall(getScriptState()));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(std::move(response));
}

TEST_F(PaymentRequestTest, ResolveShowPromiseWithoutShippingAddressInResponse)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(MockFunction::expectCall(getScriptState()), MockFunction::expectNoCall(getScriptState()));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(mojom::blink::PaymentResponse::New());
}

TEST_F(PaymentRequestTest, OnShippingOptionChange)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectNoCall(getScriptState()));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnShippingOptionChange("standardShipping");
}

TEST_F(PaymentRequestTest, CannotCallShowTwice)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState());

    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectCall(getScriptState()));
}

TEST_F(PaymentRequestTest, CannotCallCompleteTwice)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(mojom::blink::PaymentResponse::New());
    request->complete(getScriptState(), false);

    request->complete(getScriptState(), true).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectCall(getScriptState()));
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnError)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectCall(getScriptState()));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnError();
}

TEST_F(PaymentRequestTest, RejectCompletePromiseOnError)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(mojom::blink::PaymentResponse::New());

    request->complete(getScriptState(), true).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectCall(getScriptState()));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnError();
}

TEST_F(PaymentRequestTest, ResolvePromiseOnComplete)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState());
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(mojom::blink::PaymentResponse::New());

    request->complete(getScriptState(), true).then(MockFunction::expectCall(getScriptState()), MockFunction::expectNoCall(getScriptState()));

    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnComplete();
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnUpdateDetailsFailure)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectCall(getScriptState()));

    request->onUpdatePaymentDetailsFailure(ScriptValue::from(getScriptState(), "oops"));
}

TEST_F(PaymentRequestTest, RejectCompletePromiseOnUpdateDetailsFailure)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState()).then(MockFunction::expectCall(getScriptState()), MockFunction::expectNoCall(getScriptState()));
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(mojom::blink::PaymentResponse::New());

    request->complete(getScriptState(), true).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectCall(getScriptState()));

    request->onUpdatePaymentDetailsFailure(ScriptValue::from(getScriptState(), "oops"));
}

TEST_F(PaymentRequestTest, IgnoreUpdatePaymentDetailsAfterShowPromiseResolved)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState()).then(MockFunction::expectCall(getScriptState()), MockFunction::expectNoCall(getScriptState()));
    static_cast<mojom::blink::PaymentRequestClient*>(request)->OnPaymentResponse(mojom::blink::PaymentResponse::New());

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), "foo"));
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnNonPaymentDetailsUpdate)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectCall(getScriptState()));

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), "NotPaymentDetails"));
}

TEST_F(PaymentRequestTest, RejectShowPromiseOnInvalidPaymentDetailsUpdate)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectCall(getScriptState()));

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), fromJSONString(getScriptState(), "{}", getExceptionState())));
    EXPECT_FALSE(getExceptionState().hadException());
}

TEST_F(PaymentRequestTest, ClearShippingOptionOnPaymentDetailsUpdateWithoutShippingOptions)
{
    ScriptState::Scope scope(getScriptState());
    PaymentDetails details;
    details.setItems(HeapVector<PaymentItem>(1, buildPaymentItemForTest()));
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), details, options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    EXPECT_TRUE(request->shippingOption().isNull());
    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectNoCall(getScriptState()));
    String detailWithShippingOptions = "{\"items\": [{\"id\": \"total\", \"label\": \"Total\", \"amount\": {\"currencyCode\": \"USD\", \"value\": \"5.00\"}}],"
        "\"shippingOptions\": [{\"id\": \"standardShippingOption\", \"label\": \"Standard shipping\", \"amount\": {\"currencyCode\": \"USD\", \"value\": \"5.00\"}}]}";
    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), fromJSONString(getScriptState(), detailWithShippingOptions, getExceptionState())));
    EXPECT_FALSE(getExceptionState().hadException());
    EXPECT_EQ("standardShippingOption", request->shippingOption());
    String detailWithoutShippingOptions = "{\"items\": [{\"id\": \"total\", \"label\": \"Total\", \"amount\": {\"currencyCode\": \"USD\", \"value\": \"5.00\"}}]}";

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), fromJSONString(getScriptState(), detailWithoutShippingOptions, getExceptionState())));

    EXPECT_FALSE(getExceptionState().hadException());
    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, ClearShippingOptionOnPaymentDetailsUpdateWithMultipleShippingOptions)
{
    ScriptState::Scope scope(getScriptState());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectNoCall(getScriptState()));
    String detail = "{\"items\": [{\"id\": \"total\", \"label\": \"Total\", \"amount\": {\"currencyCode\": \"USD\", \"value\": \"5.00\"}}],"
        "\"shippingOptions\": [{\"id\": \"slow\", \"label\": \"Slow\", \"amount\": {\"currencyCode\": \"USD\", \"value\": \"5.00\"}},"
        "{\"id\": \"fast\", \"label\": \"Fast\", \"amount\": {\"currencyCode\": \"USD\", \"value\": \"50.00\"}}]}";

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), fromJSONString(getScriptState(), detail, getExceptionState())));
    EXPECT_FALSE(getExceptionState().hadException());

    EXPECT_TRUE(request->shippingOption().isNull());
}

TEST_F(PaymentRequestTest, UseTheSingleShippingOptionFromPaymentDetailsUpdate)
{
    ScriptState::Scope scope(getScriptState());
    PaymentOptions options;
    options.setRequestShipping(true);
    PaymentRequest* request = PaymentRequest::create(getScriptState(), Vector<String>(1, "foo"), buildPaymentDetailsForTest(), options, getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());
    request->show(getScriptState()).then(MockFunction::expectNoCall(getScriptState()), MockFunction::expectNoCall(getScriptState()));
    String detail = "{\"items\": [{\"id\": \"total\", \"label\": \"Total\", \"amount\": {\"currencyCode\": \"USD\", \"value\": \"5.00\"}}],"
        "\"shippingOptions\": [{\"id\": \"standardShippingOption\", \"label\": \"Standard shipping\", \"amount\": {\"currencyCode\": \"USD\", \"value\": \"5.00\"}}]}";

    request->onUpdatePaymentDetails(ScriptValue::from(getScriptState(), fromJSONString(getScriptState(), detail, getExceptionState())));
    EXPECT_FALSE(getExceptionState().hadException());

    EXPECT_EQ("standardShippingOption", request->shippingOption());
}

} // namespace
} // namespace blink
