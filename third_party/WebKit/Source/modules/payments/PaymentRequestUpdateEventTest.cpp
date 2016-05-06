// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestUpdateEvent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/EventTypeNames.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/payments/PaymentUpdater.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/OwnPtr.h"

namespace blink {
namespace {

class MockPaymentUpdater : public GarbageCollectedFinalized<MockPaymentUpdater>, public PaymentUpdater {
    USING_GARBAGE_COLLECTED_MIXIN(MockPaymentUpdater);
    WTF_MAKE_NONCOPYABLE(MockPaymentUpdater);

public:
    MockPaymentUpdater() {}
    ~MockPaymentUpdater() override {}

    MOCK_METHOD1(onUpdatePaymentDetails, void(const ScriptValue& detailsScriptValue));
    MOCK_METHOD1(onUpdatePaymentDetailsFailure, void(const ScriptValue& error));

    DEFINE_INLINE_TRACE() {}
};

class PaymentRequestUpdateEventTest : public testing::Test {
public:
    PaymentRequestUpdateEventTest()
        : m_page(DummyPageHolder::create())
    {
    }

    ~PaymentRequestUpdateEventTest() override {}

    ScriptState* getScriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    ExecutionContext* getExecutionContext() { return &m_page->document(); }
    ExceptionState& getExceptionState() { return m_exceptionState; }

private:
    OwnPtr<DummyPageHolder> m_page;
    TrackExceptionState m_exceptionState;
};

TEST_F(PaymentRequestUpdateEventTest, OnUpdatePaymentDetailsCalled)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create();
    MockPaymentUpdater* updater = new MockPaymentUpdater;
    event->setPaymentDetailsUpdater(updater);
    event->setEventPhase(Event::CAPTURING_PHASE);
    ScriptPromiseResolver* paymentDetails = ScriptPromiseResolver::create(getScriptState());
    event->updateWith(getScriptState(), paymentDetails->promise(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    EXPECT_CALL(*updater, onUpdatePaymentDetails(testing::_));
    EXPECT_CALL(*updater, onUpdatePaymentDetailsFailure(testing::_)).Times(0);

    paymentDetails->resolve("foo");
}

TEST_F(PaymentRequestUpdateEventTest, OnUpdatePaymentDetailsFailureCalled)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create(EventTypeNames::shippingaddresschange);
    MockPaymentUpdater* updater = new MockPaymentUpdater;
    event->setPaymentDetailsUpdater(updater);
    event->setEventPhase(Event::CAPTURING_PHASE);
    ScriptPromiseResolver* paymentDetails = ScriptPromiseResolver::create(getScriptState());
    event->updateWith(getScriptState(), paymentDetails->promise(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    EXPECT_CALL(*updater, onUpdatePaymentDetails(testing::_)).Times(0);
    EXPECT_CALL(*updater, onUpdatePaymentDetailsFailure(testing::_));

    paymentDetails->reject("oops");
}

TEST_F(PaymentRequestUpdateEventTest, CannotUpdateWithoutDispatching)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create(EventTypeNames::shippingaddresschange);
    event->setPaymentDetailsUpdater(new MockPaymentUpdater);

    event->updateWith(getScriptState(), ScriptPromiseResolver::create(getScriptState())->promise(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
}

TEST_F(PaymentRequestUpdateEventTest, CannotUpdateTwice)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create(EventTypeNames::shippingaddresschange);
    MockPaymentUpdater* updater = new MockPaymentUpdater;
    event->setPaymentDetailsUpdater(updater);
    event->setEventPhase(Event::CAPTURING_PHASE);
    event->updateWith(getScriptState(), ScriptPromiseResolver::create(getScriptState())->promise(), getExceptionState());
    EXPECT_FALSE(getExceptionState().hadException());

    event->updateWith(getScriptState(), ScriptPromiseResolver::create(getScriptState())->promise(), getExceptionState());

    EXPECT_TRUE(getExceptionState().hadException());
}

TEST_F(PaymentRequestUpdateEventTest, UpdaterNotRequired)
{
    ScriptState::Scope scope(getScriptState());
    PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::create(EventTypeNames::shippingaddresschange);

    event->updateWith(getScriptState(), ScriptPromiseResolver::create(getScriptState())->promise(), getExceptionState());

    EXPECT_FALSE(getExceptionState().hadException());
}

} // namespace
} // namespace blink
