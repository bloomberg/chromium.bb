// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/EventSource.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MessagePort.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/events/MessageEvent.h"
#include "core/loader/MockThreadableLoader.h"
#include "core/page/EventSourceInit.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <string.h>
#include <v8.h>

namespace blink {

namespace {

String dataAsString(ScriptState* scriptState, MessageEvent* event)
{
    ScriptState::Scope scope(scriptState);
    v8::Isolate* isolate = scriptState->isolate();
    ASSERT(MessageEvent::DataTypeSerializedScriptValue == event->dataType());
    if (!event->dataAsSerializedScriptValue())
        return String();
    MessagePortArray ports = event->ports();
    NonThrowableExceptionState es;
    return toUSVString(isolate, event->dataAsSerializedScriptValue()->deserialize(isolate, &ports), es);
}

class FakeEventListener : public EventListener {
public:
    static PassRefPtrWillBeRawPtr<FakeEventListener> create()
    {
        return adoptRefWillBeNoop(new FakeEventListener());
    }
    bool operator==(const EventListener& x) const override { return &x == this; }
    const WillBeHeapVector<RefPtrWillBeMember<Event>>& events() { return m_events; }
    void handleEvent(ExecutionContext* executionContext, Event* event)
    {
        m_events.append(event);
    }
    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_events);
        EventListener::trace(visitor);
    }

protected:
    FakeEventListener() : EventListener(EventListener::CPPEventListenerType) {}
    WillBeHeapVector<RefPtrWillBeMember<Event>> m_events;
};

class EventSourceTest : public ::testing::Test {
protected:
    EventSourceTest()
        : m_page(DummyPageHolder::create())
        , m_source(EventSource::create(&document(), "https://localhost/", EventSourceInit(), m_exceptionState))
    {
        source()->setStateForTest(EventSource::OPEN);
        source()->setThreadableLoaderForTest(MockThreadableLoader::create());
    }
    ~EventSourceTest() override
    {
        source()->setThreadableLoaderForTest(nullptr);
        source()->close();

        // We need this because there is
        // listener -> event -> source -> listener
        // reference cycle on non-oilpan build.
        m_source->removeAllEventListeners();
        m_source = nullptr;
    }

    void enqueue(const char* data) { client()->didReceiveData(data, strlen(data)); }
    void enqueueOneByOne(const char* data)
    {
        const char*p = data;
        while (*p != '\0')
            client()->didReceiveData(p++, 1);
    }

    Document& document() { return m_page->document(); }
    ScriptState* scriptState() { return ScriptState::forMainWorld(document().frame()); }
    EventSource* source() { return m_source; }
    ThreadableLoaderClient* client() { return m_source->asThreadableLoaderClientForTest(); }

private:
    OwnPtr<DummyPageHolder> m_page;
    NonThrowableExceptionState m_exceptionState;
    Persistent<EventSource> m_source;
};

TEST_F(EventSourceTest, EmptyMessageEventShouldNotBeDispatched)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueue("\n");

    EXPECT_EQ(0u, listener->events().size());
}

TEST_F(EventSourceTest, DispatchSimpleMessageEvent)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueue("data:hello\n\n");

    ASSERT_EQ(1u, listener->events().size());
    ASSERT_EQ("message", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event.get()));
    EXPECT_EQ(String(), event->lastEventId());
}

TEST_F(EventSourceTest, DispatchMessageEventWithLastEventId)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueue("id:99\ndata:hello\n\n");

    ASSERT_EQ(1u, listener->events().size());
    ASSERT_EQ("message", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event.get()));
    EXPECT_EQ("99", event->lastEventId());
}

TEST_F(EventSourceTest, DispatchMessageEventWithCustomEventType)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("foo", listener);
    enqueue("event:foo\ndata:hello\n\n");

    ASSERT_EQ(1u, listener->events().size());
    ASSERT_EQ("foo", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event.get()));
}

TEST_F(EventSourceTest, RetryTakesEffectEvenWhenNotDispatching)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    ASSERT_NE(999u, EventSource::defaultReconnectDelay);

    EXPECT_EQ(EventSource::defaultReconnectDelay, source()->reconnectDelayForTest());
    enqueue("retry:999\n");
    EXPECT_EQ(999u, source()->reconnectDelayForTest());
    EXPECT_EQ(0u, listener->events().size());
}

TEST_F(EventSourceTest, EventTypeShouldBeReset)
{
    RefPtrWillBeRawPtr<FakeEventListener> fooListener = FakeEventListener::create();
    RefPtrWillBeRawPtr<FakeEventListener> messageListener = FakeEventListener::create();

    source()->addEventListener("foo", fooListener);
    source()->addEventListener("message", messageListener);
    enqueue("event:foo\ndata:hello\n\ndata:bye\n\n");

    ASSERT_EQ(1u, fooListener->events().size());
    ASSERT_EQ("foo", fooListener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> fooEvent = static_cast<MessageEvent*>(fooListener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, fooEvent->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), fooEvent.get()));

    ASSERT_EQ(1u, messageListener->events().size());
    ASSERT_EQ("message", messageListener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> messageEvent = static_cast<MessageEvent*>(messageListener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, messageEvent->dataType());
    EXPECT_EQ("bye", dataAsString(scriptState(), messageEvent.get()));
}

TEST_F(EventSourceTest, DataShouldBeReset)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueue("data:hello\n\n\n");

    ASSERT_EQ(1u, listener->events().size());
    ASSERT_EQ("message", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event.get()));
}

TEST_F(EventSourceTest, LastEventIdShouldNotBeReset)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueue("id:99\ndata:hello\n\ndata:bye\n\n");

    ASSERT_EQ(2u, listener->events().size());

    ASSERT_EQ("message", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event0 = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event0->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event0.get()));
    EXPECT_EQ("99", event0->lastEventId());

    ASSERT_EQ("message", listener->events()[1]->type());
    RefPtrWillBeRawPtr<MessageEvent> event1 = static_cast<MessageEvent*>(listener->events()[1].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event1->dataType());
    EXPECT_EQ("bye", dataAsString(scriptState(), event1.get()));
    EXPECT_EQ("99", event1->lastEventId());
}

TEST_F(EventSourceTest, VariousNewLinesShouldBeAllowed)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueueOneByOne("data:hello\r\n\rdata:bye\r\r");

    ASSERT_EQ(2u, listener->events().size());

    ASSERT_EQ("message", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event0 = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event0->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event0.get()));

    ASSERT_EQ("message", listener->events()[1]->type());
    RefPtrWillBeRawPtr<MessageEvent> event1 = static_cast<MessageEvent*>(listener->events()[1].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event1->dataType());
    EXPECT_EQ("bye", dataAsString(scriptState(), event1.get()));
}

TEST_F(EventSourceTest, RetryWithEmptyValueShouldRestoreDefaultValue)
{
    // TODO(yhirano): This is unspecified in the spec. We need to update
    // the implementation or the spec.
    EXPECT_EQ(EventSource::defaultReconnectDelay, source()->reconnectDelayForTest());
    enqueue("retry: 12\n");
    EXPECT_NE(EventSource::defaultReconnectDelay, source()->reconnectDelayForTest());
    enqueue("retry\n");
    EXPECT_EQ(EventSource::defaultReconnectDelay, source()->reconnectDelayForTest());
}

TEST_F(EventSourceTest, NonDigitRetryShouldBeIgnored)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);

    EXPECT_EQ(EventSource::defaultReconnectDelay, source()->reconnectDelayForTest());
    enqueue("retry:123\n");
    EXPECT_NE(EventSource::defaultReconnectDelay, source()->reconnectDelayForTest());
    EXPECT_EQ(123u, source()->reconnectDelayForTest());

    enqueue("retry:a0\n");
    EXPECT_EQ(123u, source()->reconnectDelayForTest());
    enqueue("retry:xi\n");
    EXPECT_EQ(123u, source()->reconnectDelayForTest());

    enqueue("retry:2a\n");
    EXPECT_EQ(123u, source()->reconnectDelayForTest());

    enqueue("retry:09a\n");
    EXPECT_EQ(123u, source()->reconnectDelayForTest());

    enqueue("retry:1\b\n");
    EXPECT_EQ(123u, source()->reconnectDelayForTest());

    enqueue("retry:  1234\n");
    EXPECT_EQ(123u, source()->reconnectDelayForTest());

    enqueue("retry:456 \n");
    EXPECT_EQ(123u, source()->reconnectDelayForTest());

    EXPECT_EQ(0u, listener->events().size());
}

TEST_F(EventSourceTest, UnrecognizedFieldShouldBeIgnored)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueue("data:hello\nhoge:fuga\npiyo\n\n");

    ASSERT_EQ(1u, listener->events().size());
    ASSERT_EQ("message", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event.get()));
}

TEST_F(EventSourceTest, CommentShouldBeIgnored)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueue("data:hello\n:event:a\n\n");

    ASSERT_EQ(1u, listener->events().size());

    ASSERT_EQ("message", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event0 = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event0->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event0.get()));
}

TEST_F(EventSourceTest, BOMShouldBeIgnored)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueue("\xef\xbb\xbf" "data:hello\n\n");

    ASSERT_EQ(1u, listener->events().size());

    ASSERT_EQ("message", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event0 = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event0->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event0.get()));
}

TEST_F(EventSourceTest, ColonlessLineShouldBeTreatedAsNameOnlyField)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueue("data:hello\nevent:a\nevent\n\n");

    ASSERT_EQ(1u, listener->events().size());

    ASSERT_EQ("message", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event0 = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event0->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event0.get()));
}

TEST_F(EventSourceTest, AtMostOneLeadingSpaceCanBeSkipped)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener(" type ", listener);
    enqueue("data:  hello  \nevent:  type \n\n");

    ASSERT_EQ(1u, listener->events().size());
    ASSERT_EQ(" type ", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event->dataType());
    EXPECT_EQ(" hello  ", dataAsString(scriptState(), event.get()));
}

TEST_F(EventSourceTest, DataShouldAccumulate)
{
    RefPtrWillBeRawPtr<FakeEventListener> listener = FakeEventListener::create();

    source()->addEventListener("message", listener);
    enqueue("data:hello\ndata: world\ndata\n\n");

    ASSERT_EQ(1u, listener->events().size());
    ASSERT_EQ("message", listener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event = static_cast<MessageEvent*>(listener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event->dataType());
    EXPECT_EQ("hello\nworld\n", dataAsString(scriptState(), event.get()));
}

TEST_F(EventSourceTest, EventShouldNotAccumulate)
{
    RefPtrWillBeRawPtr<FakeEventListener> aListener = FakeEventListener::create();
    RefPtrWillBeRawPtr<FakeEventListener> bListener = FakeEventListener::create();

    source()->addEventListener("a", aListener);
    source()->addEventListener("b", bListener);
    enqueue("data:hello\nevent:a\nevent:b\n\n");

    ASSERT_EQ(0u, aListener->events().size());
    ASSERT_EQ(1u, bListener->events().size());
    ASSERT_EQ("b", bListener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> event = static_cast<MessageEvent*>(bListener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, event->dataType());
    EXPECT_EQ("hello", dataAsString(scriptState(), event.get()));
}

TEST_F(EventSourceTest, FeedDataOneByOne)
{
    RefPtrWillBeRawPtr<FakeEventListener> aListener = FakeEventListener::create();
    RefPtrWillBeRawPtr<FakeEventListener> bListener = FakeEventListener::create();
    RefPtrWillBeRawPtr<FakeEventListener> messageListener = FakeEventListener::create();

    source()->addEventListener("a", aListener);
    source()->addEventListener("b", bListener);
    source()->addEventListener("message", messageListener);
    enqueueOneByOne("data:hello\r\ndata:world\revent:a\revent:b\nid:4\n\nid:8\ndata:bye\r\n\r");

    ASSERT_EQ(0u, aListener->events().size());

    ASSERT_EQ(1u, bListener->events().size());
    ASSERT_EQ("b", bListener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> bEvent = static_cast<MessageEvent*>(bListener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, bEvent->dataType());
    EXPECT_EQ("hello\nworld", dataAsString(scriptState(), bEvent.get()));
    EXPECT_EQ("4", bEvent->lastEventId());

    ASSERT_EQ(1u, messageListener->events().size());
    ASSERT_EQ("message", messageListener->events()[0]->type());
    RefPtrWillBeRawPtr<MessageEvent> messageEvent = static_cast<MessageEvent*>(messageListener->events()[0].get());
    ASSERT_EQ(MessageEvent::DataTypeSerializedScriptValue, messageEvent->dataType());
    EXPECT_EQ("bye", dataAsString(scriptState(), messageEvent.get()));
    EXPECT_EQ("8", messageEvent->lastEventId());
}

} // namespace

} // namespace blink
