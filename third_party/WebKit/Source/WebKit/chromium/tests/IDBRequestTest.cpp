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

#include "FrameTestHelpers.h"
#include "WebFrame.h"
#include "WebFrameImpl.h"
#include "WebView.h"
#include "bindings/v8/ScriptController.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/page/Frame.h"
#include "modules/indexeddb/IDBCursorBackendInterface.h"
#include "modules/indexeddb/IDBDatabaseBackendInterface.h"
#include "modules/indexeddb/IDBDatabaseCallbacksImpl.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "modules/indexeddb/IDBTransactionCoordinator.h"

#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKit;

namespace {

class IDBRequestTest : public testing::Test {
public:
    IDBRequestTest()
        : m_webView(0)
    {
    }

    void SetUp() OVERRIDE
    {
        m_webView = FrameTestHelpers::createWebViewAndLoad("about:blank");
        m_webView->setFocus(true);
    }

    void TearDown() OVERRIDE
    {
        m_webView->close();
    }

    v8::Handle<v8::Context> context()
    {
        return static_cast<WebFrameImpl*>(m_webView->mainFrame())->frame()->script()->mainWorldContext();
    }

    ScriptExecutionContext* scriptExecutionContext()
    {
        return static_cast<WebFrameImpl*>(m_webView->mainFrame())->frame()->document();
    }

private:
    WebView* m_webView;
};

TEST_F(IDBRequestTest, EventsAfterStopping)
{
    v8::HandleScope handleScope;
    v8::Context::Scope scope(context());

    IDBTransaction* transaction = 0;
    RefPtr<IDBRequest> request = IDBRequest::create(scriptExecutionContext(), IDBAny::createInvalid(), transaction);
    EXPECT_EQ(request->readyState(), "pending");
    scriptExecutionContext()->stopActiveDOMObjects();

    // Ensure none of the following raise assertions in stopped state:
    request->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError, "Description goes here."));
    request->onSuccess(Vector<String>());
    request->onSuccess(PassRefPtr<IDBCursorBackendInterface>(), IDBKey::createInvalid(), IDBKey::createInvalid(), 0);
    request->onSuccess(IDBKey::createInvalid());
    request->onSuccess(PassRefPtr<SharedBuffer>(0));
    request->onSuccess(PassRefPtr<SharedBuffer>(0), IDBKey::createInvalid(), IDBKeyPath());
    request->onSuccess(0LL);
    request->onSuccess();
    request->onSuccess(IDBKey::createInvalid(), IDBKey::createInvalid(), 0);
}

TEST_F(IDBRequestTest, AbortErrorAfterAbort)
{
    v8::HandleScope handleScope;
    v8::Context::Scope scope(context());

    IDBTransaction* transaction = 0;
    RefPtr<IDBRequest> request = IDBRequest::create(scriptExecutionContext(), IDBAny::createInvalid(), transaction);
    EXPECT_EQ(request->readyState(), "pending");

    // Simulate the IDBTransaction having received onAbort from back end and aborting the request:
    request->abort();

    // Now simulate the back end having fired an abort error at the request to clear up any intermediaries.
    // Ensure an assertion is not raised.
    request->onError(IDBDatabaseError::create(IDBDatabaseException::AbortError, "Description goes here."));
}

class MockIDBDatabaseBackendInterface : public IDBDatabaseBackendInterface {
public:
    static PassRefPtr<MockIDBDatabaseBackendInterface> create()
    {
        return adoptRef(new MockIDBDatabaseBackendInterface());
    }
    virtual ~MockIDBDatabaseBackendInterface()
    {
        EXPECT_TRUE(m_closeCalled);
    }

    virtual void createObjectStore(int64_t transactionId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement) OVERRIDE { }
    virtual void deleteObjectStore(int64_t transactionId, int64_t objectStoreId) OVERRIDE { }
    virtual void createTransaction(int64_t transactionId, PassRefPtr<IDBDatabaseCallbacks>, const Vector<int64_t>& objectStoreIds, unsigned short mode) OVERRIDE { }
    virtual void close(PassRefPtr<IDBDatabaseCallbacks>) OVERRIDE
    {
        m_closeCalled = true;
    }

    virtual void commit(int64_t transactionId) OVERRIDE { }
    virtual void abort(int64_t transactionId) OVERRIDE { }
    virtual void abort(int64_t transactionId, PassRefPtr<IDBDatabaseError>) OVERRIDE { }

    virtual void createIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool unique, bool multiEntry) OVERRIDE { }
    virtual void deleteIndex(int64_t transactionId, int64_t objectStoreId, int64_t indexId) OVERRIDE { }

    virtual void get(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, bool keyOnly, PassRefPtr<IDBCallbacks>) OVERRIDE { }
    virtual void put(int64_t transactionId, int64_t objectStoreId, PassRefPtr<SharedBuffer> value, PassRefPtr<IDBKey>, PutMode, PassRefPtr<IDBCallbacks>, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&) OVERRIDE { }
    virtual void setIndexKeys(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKey>, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&) OVERRIDE { }
    virtual void setIndexesReady(int64_t transactionId, int64_t objectStoreId, const Vector<int64_t>& indexIds) OVERRIDE { }
    virtual void openCursor(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, IndexedDB::CursorDirection, bool keyOnly, TaskType, PassRefPtr<IDBCallbacks>) OVERRIDE { }
    virtual void count(int64_t transactionId, int64_t objectStoreId, int64_t indexId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>) OVERRIDE { }
    virtual void deleteRange(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBKeyRange>, PassRefPtr<IDBCallbacks>) OVERRIDE { }
    virtual void clear(int64_t transactionId, int64_t objectStoreId, PassRefPtr<IDBCallbacks>) OVERRIDE { }

private:
    MockIDBDatabaseBackendInterface()
        : m_closeCalled(false)
    {
    }

    bool m_closeCalled;
};

TEST_F(IDBRequestTest, ConnectionsAfterStopping)
{
    v8::HandleScope handleScope;
    v8::Context::Scope scope(context());

    const int64_t transactionId = 1234;
    const int64_t version = 1;
    const int64_t oldVersion = 0;
    const IDBDatabaseMetadata metadata;
    RefPtr<IDBDatabaseCallbacksImpl> callbacks = IDBDatabaseCallbacksImpl::create();

    {
        RefPtr<MockIDBDatabaseBackendInterface> interface = MockIDBDatabaseBackendInterface::create();
        RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(scriptExecutionContext(), callbacks, transactionId, version);
        EXPECT_EQ(request->readyState(), "pending");

        scriptExecutionContext()->stopActiveDOMObjects();
        request->onUpgradeNeeded(oldVersion, interface, metadata);
    }

    {
        RefPtr<MockIDBDatabaseBackendInterface> interface = MockIDBDatabaseBackendInterface::create();
        RefPtr<IDBOpenDBRequest> request = IDBOpenDBRequest::create(scriptExecutionContext(), callbacks, transactionId, version);
        EXPECT_EQ(request->readyState(), "pending");

        scriptExecutionContext()->stopActiveDOMObjects();
        request->onSuccess(interface, metadata);;
    }
}

} // namespace
