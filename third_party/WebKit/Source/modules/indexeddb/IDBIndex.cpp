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
#include "bindings/v8/IDBBindingUtilities.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/WebIDBCallbacksImpl.h"
#include "public/platform/WebIDBKeyRange.h"

using blink::WebIDBCallbacks;
using blink::WebIDBCursor;
using blink::WebIDBDatabase;

namespace WebCore {

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

ScriptValue IDBIndex::keyPath(ExecutionContext* context) const
{
    DOMRequestState requestState(toIsolate(context));
    return idbAnyToScriptValue(&requestState, IDBAny::create(m_metadata.keyPath));
}

PassRefPtr<IDBRequest> IDBIndex::openCursor(ExecutionContext* context, const ScriptValue& range, const String& directionString, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBIndex::openCursor");
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::indexDeletedErrorMessage);
        return nullptr;
    }
    if (m_transaction->isFinished()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return nullptr;
    }
    if (!m_transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return nullptr;
    }
    WebIDBCursor::Direction direction = IDBCursor::stringToDirection(directionString, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::fromScriptValue(context, range, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    return openCursor(context, keyRange.release(), direction);
}

PassRefPtr<IDBRequest> IDBIndex::openCursor(ExecutionContext* context, PassRefPtr<IDBKeyRange> keyRange, WebIDBCursor::Direction direction)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    request->setCursorDetails(IndexedDB::CursorKeyAndValue, direction);
    backendDB()->openCursor(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange, direction, false, WebIDBDatabase::NormalTask, WebIDBCallbacksImpl::create(request).leakPtr());
    return request;
}

PassRefPtr<IDBRequest> IDBIndex::count(ExecutionContext* context, const ScriptValue& range, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBIndex::count");
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::indexDeletedErrorMessage);
        return nullptr;
    }
    if (m_transaction->isFinished()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return nullptr;
    }
    if (!m_transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return nullptr;
    }

    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::fromScriptValue(context, range, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    backendDB()->count(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange.release(), WebIDBCallbacksImpl::create(request).leakPtr());
    return request;
}

PassRefPtr<IDBRequest> IDBIndex::openKeyCursor(ExecutionContext* context, const ScriptValue& range, const String& directionString, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBIndex::openKeyCursor");
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::indexDeletedErrorMessage);
        return nullptr;
    }
    if (m_transaction->isFinished()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return nullptr;
    }
    if (!m_transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return nullptr;
    }
    WebIDBCursor::Direction direction = IDBCursor::stringToDirection(directionString, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::fromScriptValue(context, range, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    request->setCursorDetails(IndexedDB::CursorKeyOnly, direction);
    backendDB()->openCursor(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange.release(), direction, true, WebIDBDatabase::NormalTask, WebIDBCallbacksImpl::create(request).leakPtr());
    return request;
}

PassRefPtr<IDBRequest> IDBIndex::get(ExecutionContext* context, const ScriptValue& key, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBIndex::get");
    return getInternal(context, key, exceptionState, false);
}

PassRefPtr<IDBRequest> IDBIndex::getKey(ExecutionContext* context, const ScriptValue& key, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBIndex::getKey");
    return getInternal(context, key, exceptionState, true);
}

PassRefPtr<IDBRequest> IDBIndex::getInternal(ExecutionContext* context, const ScriptValue& key, ExceptionState& exceptionState, bool keyOnly)
{
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::indexDeletedErrorMessage);
        return nullptr;
    }
    if (m_transaction->isFinished()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return nullptr;
    }
    if (!m_transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return nullptr;
    }

    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::fromScriptValue(context, key, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    if (!keyRange) {
        exceptionState.throwDOMException(DataError, IDBDatabase::noKeyOrKeyRangeErrorMessage);
        return nullptr;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    backendDB()->get(m_transaction->id(), m_objectStore->id(), m_metadata.id, keyRange.release(), keyOnly, WebIDBCallbacksImpl::create(request).leakPtr());
    return request;
}

WebIDBDatabase* IDBIndex::backendDB() const
{
    return m_transaction->backendDB();
}

bool IDBIndex::isDeleted() const
{
    return m_deleted || m_objectStore->isDeleted();
}

} // namespace WebCore
