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
#include "modules/indexeddb/IDBCursor.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/IDBBindingUtilities.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/inspector/ScriptCallStack.h"
#include "platform/SharedBuffer.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBCallbacks.h"
#include "modules/indexeddb/IDBCursorBackendInterface.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBTransaction.h"
#include <limits>

namespace WebCore {

PassRefPtr<IDBCursor> IDBCursor::create(PassRefPtr<IDBCursorBackendInterface> backend, IndexedDB::CursorDirection direction, IDBRequest* request, IDBAny* source, IDBTransaction* transaction)
{
    return adoptRef(new IDBCursor(backend, direction, request, source, transaction));
}

const AtomicString& IDBCursor::directionNext()
{
    DEFINE_STATIC_LOCAL(AtomicString, next, ("next", AtomicString::ConstructFromLiteral));
    return next;
}

const AtomicString& IDBCursor::directionNextUnique()
{
    DEFINE_STATIC_LOCAL(AtomicString, nextunique, ("nextunique", AtomicString::ConstructFromLiteral));
    return nextunique;
}

const AtomicString& IDBCursor::directionPrev()
{
    DEFINE_STATIC_LOCAL(AtomicString, prev, ("prev", AtomicString::ConstructFromLiteral));
    return prev;
}

const AtomicString& IDBCursor::directionPrevUnique()
{
    DEFINE_STATIC_LOCAL(AtomicString, prevunique, ("prevunique", AtomicString::ConstructFromLiteral));
    return prevunique;
}


IDBCursor::IDBCursor(PassRefPtr<IDBCursorBackendInterface> backend, IndexedDB::CursorDirection direction, IDBRequest* request, IDBAny* source, IDBTransaction* transaction)
    : m_backend(backend)
    , m_request(request)
    , m_direction(direction)
    , m_source(source)
    , m_transaction(transaction)
    , m_gotValue(false)
    , m_keyDirty(true)
    , m_primaryKeyDirty(true)
    , m_valueDirty(true)
{
    ASSERT(m_backend);
    ASSERT(m_request);
    ASSERT(m_source->type() == IDBAny::IDBObjectStoreType || m_source->type() == IDBAny::IDBIndexType);
    ASSERT(m_transaction);
    ScriptWrappable::init(this);
}

IDBCursor::~IDBCursor()
{
}

PassRefPtr<IDBRequest> IDBCursor::update(ScriptState* state, ScriptValue& value, ExceptionState& es)
{
    IDB_TRACE("IDBCursor::update");

    if (!m_gotValue) {
        es.throwDOMException(InvalidStateError, IDBDatabase::noValueErrorMessage);
        return 0;
    }
    if (isKeyCursor()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::isKeyCursorErrorMessage);
        return 0;
    }
    if (isDeleted()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::sourceDeletedErrorMessage);
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
    if (m_transaction->isReadOnly()) {
        es.throwUninformativeAndGenericDOMException(ReadOnlyError);
        return 0;
    }

    RefPtr<IDBObjectStore> objectStore = effectiveObjectStore();
    const IDBKeyPath& keyPath = objectStore->metadata().keyPath;
    const bool usesInLineKeys = !keyPath.isNull();
    if (usesInLineKeys) {
        RefPtr<IDBKey> keyPathKey = createIDBKeyFromScriptValueAndKeyPath(m_request->requestState(), value, keyPath);
        if (!keyPathKey || !keyPathKey->isEqual(m_primaryKey.get())) {
            es.throwDOMException(DataError, "The effective object store of this cursor uses in-line keys and evaluating the key path of the value parameter results in a different value than the cursor's effective key.");
            return 0;
        }
    }

    return objectStore->put(IDBDatabaseBackendInterface::CursorUpdate, IDBAny::create(this), state, value, m_primaryKey, es);
}

void IDBCursor::advance(unsigned long count, ExceptionState& es)
{
    IDB_TRACE("IDBCursor::advance");
    if (!m_gotValue) {
        es.throwDOMException(InvalidStateError, IDBDatabase::noValueErrorMessage);
        return;
    }
    if (isDeleted()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::sourceDeletedErrorMessage);
        return;
    }

    if (m_transaction->isFinished()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return;
    }
    if (!m_transaction->isActive()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return;
    }

    if (!count) {
        es.throwTypeError();
        return;
    }

    m_request->setPendingCursor(this);
    m_gotValue = false;
    m_backend->advance(count, m_request);
}

void IDBCursor::continueFunction(ScriptExecutionContext* context, const ScriptValue& keyValue, ExceptionState& es)
{
    DOMRequestState requestState(context);
    RefPtr<IDBKey> key = keyValue.isUndefined() ? 0 : scriptValueToIDBKey(&requestState, keyValue);
    continueFunction(key.release(), es);
}

void IDBCursor::continueFunction(PassRefPtr<IDBKey> key, ExceptionState& es)
{
    IDB_TRACE("IDBCursor::continue");
    if (key && !key->isValid()) {
        es.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return;
    }

    if (m_transaction->isFinished()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return;
    }
    if (!m_transaction->isActive()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return;
    }

    if (!m_gotValue) {
        es.throwDOMException(InvalidStateError, IDBDatabase::noValueErrorMessage);
        return;
    }

    if (isDeleted()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::sourceDeletedErrorMessage);
        return;
    }

    if (key) {
        ASSERT(m_key);
        if (m_direction == IndexedDB::CursorNext || m_direction == IndexedDB::CursorNextNoDuplicate) {
            if (!m_key->isLessThan(key.get())) {
                es.throwDOMException(DataError, "The parameter is less than or equal to this cursor's position.");
                return;
            }
        } else {
            if (!key->isLessThan(m_key.get())) {
                es.throwDOMException(DataError, "The parameter is greater than or equal to this cursor's position.");
                return;
            }
        }
    }

    // FIXME: We're not using the context from when continue was called, which means the callback
    //        will be on the original context openCursor was called on. Is this right?
    m_request->setPendingCursor(this);
    m_gotValue = false;
    m_backend->continueFunction(key, m_request);
}

PassRefPtr<IDBRequest> IDBCursor::deleteFunction(ScriptExecutionContext* context, ExceptionState& es)
{
    IDB_TRACE("IDBCursor::delete");
    if (m_transaction->isFinished()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return 0;
    }
    if (!m_transaction->isActive()) {
        es.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return 0;
    }
    if (m_transaction->isReadOnly()) {
        es.throwUninformativeAndGenericDOMException(ReadOnlyError);
        return 0;
    }

    if (!m_gotValue) {
        es.throwDOMException(InvalidStateError, IDBDatabase::noValueErrorMessage);
        return 0;
    }
    if (isKeyCursor()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::isKeyCursorErrorMessage);
        return 0;
    }
    if (isDeleted()) {
        es.throwDOMException(InvalidStateError, IDBDatabase::sourceDeletedErrorMessage);
        return 0;
    }

    RefPtr<IDBKeyRange> keyRange = IDBKeyRange::only(m_primaryKey, es);
    ASSERT(!es.hadException());

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    m_transaction->backendDB()->deleteRange(m_transaction->id(), effectiveObjectStore()->id(), keyRange, request);
    return request.release();
}

void IDBCursor::postSuccessHandlerCallback()
{
    if (m_backend)
        m_backend->postSuccessHandlerCallback();
}

void IDBCursor::close()
{
    // The notifier may be the last reference to this cursor.
    RefPtr<IDBCursor> protect(this);
    m_request.clear();
    m_backend.clear();
}

void IDBCursor::checkForReferenceCycle()
{
    // If this cursor and its request have the only references
    // to each other, then explicitly break the cycle.
    if (!m_request || m_request->getResultCursor() != this)
        return;

    if (!hasOneRef() || !m_request->hasOneRef())
        return;

    m_request.clear();
}

ScriptValue IDBCursor::key(ScriptExecutionContext* context)
{
    m_keyDirty = false;
    DOMRequestState requestState(context);
    return idbKeyToScriptValue(&requestState, m_key);
}

ScriptValue IDBCursor::primaryKey(ScriptExecutionContext* context)
{
    m_primaryKeyDirty = false;
    DOMRequestState requestState(context);
    return idbKeyToScriptValue(&requestState, m_primaryKey);
}

ScriptValue IDBCursor::value(ScriptExecutionContext* context)
{
    ASSERT(isCursorWithValue());

    m_valueDirty = false;
    DOMRequestState requestState(context);
    ScriptValue value = deserializeIDBValueBuffer(&requestState, m_value);
    RefPtr<IDBObjectStore> objectStore = effectiveObjectStore();
    const IDBObjectStoreMetadata metadata = objectStore->metadata();
    if (metadata.autoIncrement && !metadata.keyPath.isNull()) {
#ifndef NDEBUG
        RefPtr<IDBKey> expectedKey = createIDBKeyFromScriptValueAndKeyPath(&requestState, value, metadata.keyPath);
        ASSERT(!expectedKey || expectedKey->isEqual(m_primaryKey.get()));
#endif
        bool injected = injectIDBKeyIntoScriptValue(&requestState, m_primaryKey, value, metadata.keyPath);
        ASSERT_UNUSED(injected, injected);
    }

    return value;
}

void IDBCursor::setValueReady(PassRefPtr<IDBKey> key, PassRefPtr<IDBKey> primaryKey, PassRefPtr<SharedBuffer> value)
{
    m_key = key;
    m_keyDirty = true;

    m_primaryKey = primaryKey;
    m_primaryKeyDirty = true;

    if (isCursorWithValue()) {
        m_value = value;
        m_valueDirty = true;
    }

    m_gotValue = true;
}

PassRefPtr<IDBObjectStore> IDBCursor::effectiveObjectStore()
{
    if (m_source->type() == IDBAny::IDBObjectStoreType)
        return m_source->idbObjectStore();
    RefPtr<IDBIndex> index = m_source->idbIndex();
    return index->objectStore();
}

bool IDBCursor::isDeleted() const
{
    if (m_source->type() == IDBAny::IDBObjectStoreType)
        return m_source->idbObjectStore()->isDeleted();
    return m_source->idbIndex()->isDeleted();
}

IndexedDB::CursorDirection IDBCursor::stringToDirection(const String& directionString, ExceptionState& es)
{
    if (directionString.isNull() || directionString == IDBCursor::directionNext())
        return IndexedDB::CursorNext;
    if (directionString == IDBCursor::directionNextUnique())
        return IndexedDB::CursorNextNoDuplicate;
    if (directionString == IDBCursor::directionPrev())
        return IndexedDB::CursorPrev;
    if (directionString == IDBCursor::directionPrevUnique())
        return IndexedDB::CursorPrevNoDuplicate;

    es.throwTypeError();
    return IndexedDB::CursorNext;
}

const AtomicString& IDBCursor::directionToString(unsigned short direction)
{
    switch (direction) {
    case IndexedDB::CursorNext:
        return IDBCursor::directionNext();

    case IndexedDB::CursorNextNoDuplicate:
        return IDBCursor::directionNextUnique();

    case IndexedDB::CursorPrev:
        return IDBCursor::directionPrev();

    case IndexedDB::CursorPrevNoDuplicate:
        return IDBCursor::directionPrevUnique();

    default:
        ASSERT_NOT_REACHED();
        return IDBCursor::directionNext();
    }
}

} // namespace WebCore
