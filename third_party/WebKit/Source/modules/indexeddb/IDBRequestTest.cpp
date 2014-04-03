/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/indexeddb/IDBRequest.h"

#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "core/events/EventQueue.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "platform/SharedBuffer.h"
#include "public/platform/WebIDBDatabase.h"
#include "wtf/PassOwnPtr.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class NullEventQueue FINAL : public EventQueue {
public:
    NullEventQueue() { }
    virtual ~NullEventQueue() { }
    virtual bool enqueueEvent(PassRefPtr<Event>) OVERRIDE { return true; }
    virtual bool cancelEvent(Event*) OVERRIDE { return true; }
    virtual void close() OVERRIDE { }
};

class NullExecutionContext FINAL : public ExecutionContext, public RefCounted<NullExecutionContext> {
public:
    using RefCounted<NullExecutionContext>::ref;
    using RefCounted<NullExecutionContext>::deref;

    virtual void refExecutionContext() OVERRIDE { ref(); }
    virtual void derefExecutionContext() OVERRIDE { deref(); }
    virtual EventQueue* eventQueue() const OVERRIDE { return m_queue.get(); }

    NullExecutionContext();
private:
    OwnPtr<EventQueue> m_queue;
};

NullExecutionContext::NullExecutionContext()
    : m_queue(adoptPtr(new NullEventQueue()))
{
}

class IDBRequestTest : public testing::Test {
public:
    IDBRequestTest()
        : m_scope(V8ExecutionScope::create(v8::Isolate::GetCurrent()))
        , m_context(adoptRef(new NullExecutionContext()))
    {
    }

    ExecutionContext* executionContext()
    {
        return m_context.get();
    }

private:
    OwnPtr<V8ExecutionScope> m_scope;
    RefPtr<ExecutionContext> m_context;
};

TEST_F(IDBRequestTest, EventsAfterStopping)
{
    IDBTransaction* transaction = 0;
    RefPtr<IDBRequest> request = IDBRequest::create(executionContext(), IDBAny::createUndefined(), transaction);
    EXPECT_EQ(request->readyState(), "pending");
    executionContext()->stopActiveDOMObjects();

    // Ensure none of the following raise assertions in stopped state:
    request->onError(DOMError::create(AbortError, "Description goes here."));
    request->onSuccess(Vector<String>());
    request->onSuccess(nullptr, IDBKey::createInvalid(), IDBKey::createInvalid(), nullptr);
    request->onSuccess(IDBKey::createInvalid());
    request->onSuccess(PassRefPtr<SharedBuffer>(nullptr));
    request->onSuccess(PassRefPtr<SharedBuffer>(nullptr), IDBKey::createInvalid(), IDBKeyPath());
    request->onSuccess(0LL);
    request->onSuccess();
    request->onSuccess(IDBKey::createInvalid(), IDBKey::createInvalid(), nullptr);
}

TEST_F(IDBRequestTest, AbortErrorAfterAbort)
{
    IDBTransaction* transaction = 0;
    RefPtr<IDBRequest> request = IDBRequest::create(executionContext(), IDBAny::createUndefined(), transaction);
    EXPECT_EQ(request->readyState(), "pending");

    // Simulate the IDBTransaction having received onAbort from back end and aborting the request:
    request->abort();

    // Now simulate the back end having fired an abort error at the request to clear up any intermediaries.
    // Ensure an assertion is not raised.
    request->onError(DOMError::create(AbortError, "Description goes here."));
}

class MockWebIDBDatabase : public blink::WebIDBDatabase {
public:
    static PassOwnPtr<MockWebIDBDatabase> create()
    {
        return adoptPtr(new MockWebIDBDatabase());
    }
    virtual ~MockWebIDBDatabase()
    {
        EXPECT_TRUE(m_closeCalled);
    }

    virtual void close() OVERRIDE
    {
        m_closeCalled = true;
    }
    virtual void abort(long long transactionId) OVERRIDE { }

private:
    MockWebIDBDatabase()
        : m_closeCalled(false)
    {
    }

    bool m_closeCalled;
};

TEST_F(IDBRequestTest, ConnectionsAfterStopping)
{
    const int64_t transactionId = 1234;
    const int64_t version = 1;
    const int64_t oldVersion = 0;
    const IDBDatabaseMetadata metadata;
    RefPtr<IDBDatabaseCallbacks> callbacks = IDBDatabaseCallbacks::create();

    {
        OwnPtr<MockWebIDBDatabase> backend = MockWebIDBDatabase::create();
        RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(executionContext(), callbacks, transactionId, version);
        EXPECT_EQ(request->readyState(), "pending");

        executionContext()->stopActiveDOMObjects();
        request->onUpgradeNeeded(oldVersion, backend.release(), metadata, blink::WebIDBDataLossNone, String());
    }

    {
        OwnPtr<MockWebIDBDatabase> backend = MockWebIDBDatabase::create();
        RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(executionContext(), callbacks, transactionId, version);
        EXPECT_EQ(request->readyState(), "pending");

        executionContext()->stopActiveDOMObjects();
        request->onSuccess(backend.release(), metadata);
    }
}

} // namespace
