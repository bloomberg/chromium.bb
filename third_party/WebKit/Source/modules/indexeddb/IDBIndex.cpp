/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "modules/indexeddb/IDBIndex.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ScriptExecutionContext.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBTransaction.h"

namespace WebCore {

static const unsigned short defaultDirection = IndexedDB::CursorNext;

IDBIndex::IDBIndex(const IDBIndexMetadata& metadata, IDBObjectStore* objectStore, IDBTransaction* transaction)
    : m_metadata(metadata)
    , m_objectStore(objectStore)
    , m_transaction(transaction)
    , m_deleted(false)
{
    ASSERT(m_objectStore);
    ASSERT(m_transaction);
    ASSERT(m_metadata.id != IDBIndexMetadata::InvalidId);
    ScriptWrappable::init(this);
}

IDBIndex::~IDBIndex()
{
}

PassRefPtr<IDBRequest> IDBIndex::openCursor(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> keyRange, const String& directionString, ExceptionState& es)
{
    IDB_TRACE("IDBIndex::openCursor");
    if (isDeleted()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::indexDeletedErrorMessage);
        return 0;
    }
    if (m_transaction->isFinished()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return 0;
    }
    if (!m_transaction->isActive()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return 0;
    }
    IndexedDB::CursorDirection direction = IDBCursor::stringToDirection(directionString, es);
    if (es.hadException())
        return 0;

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    request->setCursorDetails(IndexedDB::CursorKeyAndValue, direction);
    backendDB()->openCursor(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, direction, false, IDBDatabaseBackendInterface::NormalTask, request);
    return request;
}

PassRefPtr<IDBRequest> IDBIndex::openCursor(ScriptExecutionContext* context, const ScriptValue& key, const String& direction, ExceptionState& es)
{
    IDB_TRACE("IDBIndex::openCursor");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, es);
    if (es.hadException())
        return 0;
    return openCursor(context, keyRange.release(), direction, es);
}

PassRefPtr<IDBRequest> IDBIndex::count(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> keyRange, ExceptionState& es)
{
    IDB_TRACE("IDBIndex::count");
    if (isDeleted()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::indexDeletedErrorMessage);
        return 0;
    }
    if (m_transaction->isFinished()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return 0;
    }
    if (!m_transaction->isActive()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return 0;
    }
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    backendDB()->count(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, request);
    return request;
}

PassRefPtr<IDBRequest> IDBIndex::count(ScriptExecutionContext* context, const ScriptValue& key, ExceptionState& es)
{
    IDB_TRACE("IDBIndex::count");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, es);
    if (es.hadException())
        return 0;
    return count(context, keyRange.release(), es);
}

PassRefPtr<IDBRequest> IDBIndex::openKeyCursor(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> keyRange, const String& directionString, ExceptionState& es)
{
    IDB_TRACE("IDBIndex::openKeyCursor");
    if (isDeleted()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::indexDeletedErrorMessage);
        return 0;
    }
    if (m_transaction->isFinished()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return 0;
    }
    if (!m_transaction->isActive()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return 0;
    }
    IndexedDB::CursorDirection direction = IDBCursor::stringToDirection(directionString, es);
    if (es.hadException())
        return 0;

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    request->setCursorDetails(IndexedDB::CursorKeyOnly, direction);
    backendDB()->openCursor(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, direction, true, IDBDatabaseBackendInterface::NormalTask, request);
    return request;
}

PassRefPtr<IDBRequest> IDBIndex::openKeyCursor(ScriptExecutionContext* context, const ScriptValue& key, const String& direction, ExceptionState& es)
{
    IDB_TRACE("IDBIndex::openKeyCursor");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, es);
    if (es.hadException())
        return 0;
    return openKeyCursor(context, keyRange.release(), direction, es);
}

PassRefPtr<IDBRequest> IDBIndex::get(ScriptExecutionContext* context, const ScriptValue& key, ExceptionState& es)
{
    IDB_TRACE("IDBIndex::get");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, es);
    if (es.hadException())
        return 0;
    return get(context, keyRange.release(), es);
}

PassRefPtr<IDBRequest> IDBIndex::get(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> keyRange, ExceptionState& es)
{
    IDB_TRACE("IDBIndex::get");
    if (isDeleted()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::indexDeletedErrorMessage);
        return 0;
    }
    if (m_transaction->isFinished()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return 0;
    }
    if (!m_transaction->isActive()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return 0;
    }
    if (!keyRange) {
        es.throwDOMException(DataError, IDBDatabase::noKeyOrKeyRangeErrorMessage);
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    backendDB()->get(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, false, request);
    return request;
}

PassRefPtr<IDBRequest> IDBIndex::getKey(ScriptExecutionContext* context, const ScriptValue& key, ExceptionState& es)
{
    IDB_TRACE("IDBIndex::getKey");
    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(context, key, es);
    if (es.hadException())
        return 0;

    return getKey(context, keyRange.release(), es);
}

PassRefPtr<IDBRequest> IDBIndex::getKey(ScriptExecutionContext* context, PassRefPtr<IDBKeyRange> keyRange, ExceptionState& es)
{
    IDB_TRACE("IDBIndex::getKey");
    if (isDeleted()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::indexDeletedErrorMessage);
        return 0;
    }
    if (m_transaction->isFinished()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return 0;
    }
    if (!m_transaction->isActive()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return 0;
    }
    if (!keyRange) {
        es.throwDOMException(DataError, IDBDatabase::noKeyOrKeyRangeErrorMessage);
        return 0;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    backendDB()->get(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, true, request);
    return request;
}

IDBDatabaseBackendInterface* IDBIndex::backendDB() const
{
    return m_transaction->backendDB();
}

bool IDBIndex::isDeleted() const
{
    return m_deleted || m_objectStore->isDeleted();
}

} // namespace WebCore
