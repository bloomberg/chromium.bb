// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentResponse.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/payments/PaymentCompleter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/OwnPtr.h"
#include <utility>

namespace blink {
namespace {

class MockPaymentCompleter : public GarbageCollectedFinalized<MockPaymentCompleter>, public PaymentCompleter {
    USING_GARBAGE_COLLECTED_MIXIN(MockPaymentCompleter);
    WTF_MAKE_NONCOPYABLE(MockPaymentCompleter);

public:
    MockPaymentCompleter()
    {
        ON_CALL(*this, complete(testing::_, testing::_))
            .WillByDefault(testing::ReturnPointee(&m_dummyPromise));
    }

    ~MockPaymentCompleter() override {}

    MOCK_METHOD2(complete, ScriptPromise(ScriptState*, bool success));

    DEFINE_INLINE_TRACE() {}

private:
    ScriptPromise m_dummyPromise;
};

class PaymentResponseTest : public testing::Test {
public:
    PaymentResponseTest()
        : m_page(DummyPageHolder::create())
    {
    }

    ~PaymentResponseTest() override {}

    ScriptState* getScriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    ExceptionState& getExceptionState() { return m_exceptionState; }

private:
    OwnPtr<DummyPageHolder> m_page;
    NonThrowableExceptionState m_exceptionState;
};

TEST_F(PaymentResponseTest, DataCopiedOver)
{
    ScriptState::Scope scope(getScriptState());
    mojom::blink::PaymentResponsePtr input = mojom::blink::PaymentResponse::New();
    input->method_name = "foo";
    input->stringified_details = "{\"transactionId\": 123}";
    MockPaymentCompleter* completeCallback = new MockPaymentCompleter;

    PaymentResponse output(std::move(input), completeCallback);

    EXPECT_EQ("foo", output.methodName());

    ScriptValue details = output.details(getScriptState(), getExceptionState());

    ASSERT_FALSE(getExceptionState().hadException());
    ASSERT_TRUE(details.v8Value()->IsObject());

    ScriptValue transactionId(getScriptState(), details.v8Value().As<v8::Object>()->Get(v8String(getScriptState()->isolate(), "transactionId")));

    ASSERT_TRUE(transactionId.v8Value()->IsNumber());
    EXPECT_EQ(123, transactionId.v8Value().As<v8::Number>()->Value());
}

TEST_F(PaymentResponseTest, CompleteCalledWithSuccess)
{
    mojom::blink::PaymentResponsePtr input = mojom::blink::PaymentResponse::New();
    input->method_name = "foo";
    input->stringified_details = "{\"transactionId\": 123}";
    MockPaymentCompleter* completeCallback = new MockPaymentCompleter;
    PaymentResponse output(std::move(input), completeCallback);

    EXPECT_CALL(*completeCallback, complete(getScriptState(), true));

    output.complete(getScriptState(), true);
}

TEST_F(PaymentResponseTest, CompleteCalledWithFailure)
{
    mojom::blink::PaymentResponsePtr input = mojom::blink::PaymentResponse::New();
    input->method_name = "foo";
    input->stringified_details = "{\"transactionId\": 123}";
    MockPaymentCompleter* completeCallback = new MockPaymentCompleter;
    PaymentResponse output(std::move(input), completeCallback);

    EXPECT_CALL(*completeCallback, complete(getScriptState(), false));

    output.complete(getScriptState(), false);
}

} // namespace
} // namespace blink
