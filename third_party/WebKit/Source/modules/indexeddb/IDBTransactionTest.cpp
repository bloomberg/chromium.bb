/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/indexeddb/IDBTransaction.h"

#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBPendingTransactionMonitor.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class IDBTransactionTest : public testing::Test {
public:
    IDBTransactionTest()
        : m_handleScope(v8::Isolate::GetCurrent())
        , m_scope(v8::Context::New(v8::Isolate::GetCurrent()))
        , m_document(Document::create())
    {
    }

    ScriptExecutionContext* scriptExecutionContext()
    {
        return m_document.get();
    }

private:
    v8::HandleScope m_handleScope;
    v8::Context::Scope m_scope;
    RefPtr<Document> m_document;
};

class FakeIDBDatabaseBackendProxy : public IDBDatabaseBackendInterface {
public:
    static PassRefPtr<FakeIDBDatabaseBackendProxy> create() { return adoptRef(new FakeIDBDatabaseBackendProxy()); }

    virtual void createObjectStore(int64_t transactionId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement) OVERRIDE { }
    virtual void deleteObjectStore(int64_t transactionId, int64_t objectStoreId) OVERRIDE { }
    virtual void createTransaction(int64_t transactionId, PassRefPtr<IDBDatabaseCallbacks>, const Vector<int64_t>& objectStoreIds, unsigned short mode) OVERRIDE { }
    virtual void close(PassRefPtr<IDBDatabaseCallbacks>) OVERRIDE { }

    virtual void commit(int64_t transactionId) OVERRIDE { }
    virtual void abort(int64_t transactionId) OVERRIDE { }

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
    FakeIDBDatabaseBackendProxy() { }
};

class FakeIDBDatabaseCallbacks : public IDBDatabaseCallbacks {
public:
    static PassRefPtr<FakeIDBDatabaseCallbacks> create() { return adoptRef(new FakeIDBDatabaseCallbacks()); }
    virtual void onVersionChange(int64_t oldVersion, int64_t newVersion) OVERRIDE { }
    virtual void onForcedClose() OVERRIDE { }
    virtual void onAbort(int64_t transactionId, PassRefPtr<DOMError> error) OVERRIDE { }
    virtual void onComplete(int64_t transactionId) OVERRIDE { }
private:
    FakeIDBDatabaseCallbacks() { }
};

TEST_F(IDBTransactionTest, EnsureLifetime)
{
    RefPtr<FakeIDBDatabaseBackendProxy> proxy = FakeIDBDatabaseBackendProxy::create();
    RefPtr<FakeIDBDatabaseCallbacks> connection = FakeIDBDatabaseCallbacks::create();
    RefPtr<IDBDatabase> db = IDBDatabase::create(scriptExecutionContext(), proxy, connection);

    const int64_t transactionId = 1234;
    const Vector<String> transactionScope;
    RefPtr<IDBTransaction> transaction = IDBTransaction::create(scriptExecutionContext(), transactionId, transactionScope, IndexedDB::TransactionReadOnly, db.get());

    // Local reference, IDBDatabase's reference and IDBPendingTransactionMonitor's reference:
    EXPECT_EQ(3, transaction->refCount());

    RefPtr<IDBRequest> request = IDBRequest::create(scriptExecutionContext(), IDBAny::createInvalid(), transaction.get());
    IDBPendingTransactionMonitor::deactivateNewTransactions();

    // Local reference, IDBDatabase's reference, and the IDBRequest's reference
    EXPECT_EQ(3, transaction->refCount());

    // This will generate an abort() call to the back end which is dropped by the fake proxy,
    // so an explicit onAbort call is made.
    scriptExecutionContext()->stopActiveDOMObjects();
    transaction->onAbort(DOMError::create(AbortError, "Aborted"));

    EXPECT_EQ(1, transaction->refCount());
}

TEST_F(IDBTransactionTest, TransactionFinish)
{
    RefPtr<FakeIDBDatabaseBackendProxy> proxy = FakeIDBDatabaseBackendProxy::create();
    RefPtr<FakeIDBDatabaseCallbacks> connection = FakeIDBDatabaseCallbacks::create();
    RefPtr<IDBDatabase> db = IDBDatabase::create(scriptExecutionContext(), proxy, connection);

    const int64_t transactionId = 1234;
    const Vector<String> transactionScope;
    RefPtr<IDBTransaction> transaction = IDBTransaction::create(scriptExecutionContext(), transactionId, transactionScope, IndexedDB::TransactionReadOnly, db.get());

    // Local reference, IDBDatabase's reference and IDBPendingTransactionMonitor's reference:
    EXPECT_EQ(3, transaction->refCount());

    IDBPendingTransactionMonitor::deactivateNewTransactions();

    // Local reference, IDBDatabase's reference
    EXPECT_EQ(2, transaction->refCount());

    IDBTransaction* transactionPtr = transaction.get();
    transaction.clear();

    // IDBDatabase's reference
    EXPECT_EQ(1, transactionPtr->refCount());

    // Stop the context, so events don't get queued (which would keep the transaction alive).
    scriptExecutionContext()->stopActiveDOMObjects();

    // Fire an abort to make sure this doesn't free the transaction during use. The test
    // will not fail if it is, but ASAN would notice the error.
    db->onAbort(transactionId, DOMError::create(AbortError, "Aborted"));
}

} // namespace
