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
#include "modules/indexeddb/IDBObjectStore.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/IDBBindingUtilities.h"
#include "bindings/v8/ScriptState.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBCursorWithValue.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/WebIDBCallbacksImpl.h"
#include "platform/SharedBuffer.h"
#include "public/platform/WebData.h"
#include "public/platform/WebIDBKey.h"
#include "public/platform/WebIDBKeyRange.h"

using blink::WebIDBCallbacks;
using blink::WebIDBCursor;
using blink::WebIDBDatabase;

namespace WebCore {

IDBObjectStore::IDBObjectStore(const IDBObjectStoreMetadata& metadata, IDBTransaction* transaction)
    : m_metadata(metadata)
    , m_transaction(transaction)
    , m_deleted(false)
{
    ASSERT(m_transaction);
    // We pass a reference to this object before it can be adopted.
    relaxAdoptionRequirement();
    ScriptWrappable::init(this);
}

ScriptValue IDBObjectStore::keyPath(ExecutionContext* context) const
{
    DOMRequestState requestState(toIsolate(context));
    return idbAnyToScriptValue(&requestState, IDBAny::create(m_metadata.keyPath));
}

PassRefPtr<DOMStringList> IDBObjectStore::indexNames() const
{
    IDB_TRACE("IDBObjectStore::indexNames");
    RefPtr<DOMStringList> indexNames = DOMStringList::create();
    for (IDBObjectStoreMetadata::IndexMap::const_iterator it = m_metadata.indexes.begin(); it != m_metadata.indexes.end(); ++it)
        indexNames->append(it->value.name);
    indexNames->sort();
    return indexNames.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::get(ExecutionContext* context, const ScriptValue& key, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::get");
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
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
    backendDB()->get(m_transaction->id(), id(), IDBIndexMetadata::InvalidId, keyRange.release(), false, WebIDBCallbacksImpl::create(request).leakPtr());
    return request.release();
}

static void generateIndexKeysForValue(DOMRequestState* requestState, const IDBIndexMetadata& indexMetadata, const ScriptValue& objectValue, IDBObjectStore::IndexKeys* indexKeys)
{
    ASSERT(indexKeys);
    RefPtr<IDBKey> indexKey = createIDBKeyFromScriptValueAndKeyPath(requestState, objectValue, indexMetadata.keyPath);

    if (!indexKey)
        return;

    if (!indexMetadata.multiEntry || indexKey->type() != IDBKey::ArrayType) {
        if (!indexKey->isValid())
            return;

        indexKeys->append(indexKey);
    } else {
        ASSERT(indexMetadata.multiEntry);
        ASSERT(indexKey->type() == IDBKey::ArrayType);
        indexKey = IDBKey::createMultiEntryArray(indexKey->array());

        for (size_t i = 0; i < indexKey->array().size(); ++i)
            indexKeys->append(indexKey->array()[i]);
    }
}

PassRefPtr<IDBRequest> IDBObjectStore::add(ScriptState* state, ScriptValue& value, const ScriptValue& key, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::add");
    return put(WebIDBDatabase::AddOnly, IDBAny::create(this), state, value, key, exceptionState);
}

PassRefPtr<IDBRequest> IDBObjectStore::put(ScriptState* state, ScriptValue& value, const ScriptValue& key, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::put");
    return put(WebIDBDatabase::AddOrUpdate, IDBAny::create(this), state, value, key, exceptionState);
}

PassRefPtr<IDBRequest> IDBObjectStore::put(WebIDBDatabase::PutMode putMode, PassRefPtr<IDBAny> source, ScriptState* state, ScriptValue& value, const ScriptValue& keyValue, ExceptionState& exceptionState)
{
    ExecutionContext* context = state->executionContext();
    DOMRequestState requestState(toIsolate(context));
    RefPtr<IDBKey> key = keyValue.isUndefined() ? nullptr : scriptValueToIDBKey(&requestState, keyValue);
    return put(putMode, source, state, value, key.release(), exceptionState);
}

PassRefPtr<IDBRequest> IDBObjectStore::put(WebIDBDatabase::PutMode putMode, PassRefPtr<IDBAny> source, ScriptState* state, ScriptValue& value, PassRefPtr<IDBKey> prpKey, ExceptionState& exceptionState)
{
    RefPtr<IDBKey> key = prpKey;
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
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
    if (m_transaction->isReadOnly()) {
        exceptionState.throwDOMException(ReadOnlyError, IDBDatabase::transactionReadOnlyErrorMessage);
        return nullptr;
    }

    RefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::create(value, 0, exceptionState, state);
    if (exceptionState.hadException())
        return nullptr;

    if (serializedValue->containsBlobs()) {
        // FIXME: Add Blob/File/FileList support
        exceptionState.throwDOMException(DataCloneError, "The object store currently does not support blob values.");
        return nullptr;
    }

    const IDBKeyPath& keyPath = m_metadata.keyPath;
    const bool usesInLineKeys = !keyPath.isNull();
    const bool hasKeyGenerator = autoIncrement();

    ExecutionContext* context = state->executionContext();
    DOMRequestState requestState(toIsolate(context));

    if (putMode != WebIDBDatabase::CursorUpdate && usesInLineKeys && key) {
        exceptionState.throwDOMException(DataError, "The object store uses in-line keys and the key parameter was provided.");
        return nullptr;
    }
    if (!usesInLineKeys && !hasKeyGenerator && !key) {
        exceptionState.throwDOMException(DataError, "The object store uses out-of-line keys and has no key generator and the key parameter was not provided.");
        return nullptr;
    }
    if (usesInLineKeys) {
        RefPtr<IDBKey> keyPathKey = createIDBKeyFromScriptValueAndKeyPath(&requestState, value, keyPath);
        if (keyPathKey && !keyPathKey->isValid()) {
            exceptionState.throwDOMException(DataError, "Evaluating the object store's key path yielded a value that is not a valid key.");
            return nullptr;
        }
        if (!hasKeyGenerator && !keyPathKey) {
            exceptionState.throwDOMException(DataError, "Evaluating the object store's key path did not yield a value.");
            return nullptr;
        }
        if (hasKeyGenerator && !keyPathKey) {
            if (!canInjectIDBKeyIntoScriptValue(&requestState, value, keyPath)) {
                exceptionState.throwDOMException(DataError, "A generated key could not be inserted into the value.");
                return nullptr;
            }
        }
        if (keyPathKey)
            key = keyPathKey;
    }
    if (key && !key->isValid()) {
        exceptionState.throwDOMException(DataError, IDBDatabase::notValidKeyErrorMessage);
        return nullptr;
    }

    Vector<int64_t> indexIds;
    Vector<IndexKeys> indexKeys;
    for (IDBObjectStoreMetadata::IndexMap::const_iterator it = m_metadata.indexes.begin(); it != m_metadata.indexes.end(); ++it) {
        IndexKeys keys;
        generateIndexKeysForValue(&requestState, it->value, value, &keys);
        indexIds.append(it->key);
        indexKeys.append(keys);
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, source, m_transaction.get());
    Vector<char> wireBytes;
    serializedValue->toWireBytes(wireBytes);
    RefPtr<SharedBuffer> valueBuffer = SharedBuffer::adoptVector(wireBytes);
    backendDB()->put(m_transaction->id(), id(), blink::WebData(valueBuffer), key.release(), static_cast<WebIDBDatabase::PutMode>(putMode), WebIDBCallbacksImpl::create(request).leakPtr(), indexIds, indexKeys);
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::deleteFunction(ExecutionContext* context, const ScriptValue& key, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::delete");
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
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
    if (m_transaction->isReadOnly()) {
        exceptionState.throwDOMException(ReadOnlyError, IDBDatabase::transactionReadOnlyErrorMessage);
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
    backendDB()->deleteRange(m_transaction->id(), id(), keyRange.release(), WebIDBCallbacksImpl::create(request).leakPtr());
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::clear(ExecutionContext* context, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::clear");
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
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
    if (m_transaction->isReadOnly()) {
        exceptionState.throwDOMException(ReadOnlyError, IDBDatabase::transactionReadOnlyErrorMessage);
        return nullptr;
    }

    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    backendDB()->clear(m_transaction->id(), id(), WebIDBCallbacksImpl::create(request).leakPtr());
    return request.release();
}

namespace {
// This class creates the index keys for a given index by extracting
// them from the SerializedScriptValue, for all the existing values in
// the objectStore. It only needs to be kept alive by virtue of being
// a listener on an IDBRequest object, in the same way that JavaScript
// cursor success handlers are kept alive.
class IndexPopulator FINAL : public EventListener {
public:
    static PassRefPtr<IndexPopulator> create(PassRefPtr<IDBDatabase> database, int64_t transactionId, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
    {
        return adoptRef(new IndexPopulator(database, transactionId, objectStoreId, indexMetadata));
    }

    virtual bool operator==(const EventListener& other) OVERRIDE
    {
        return this == &other;
    }

private:
    IndexPopulator(PassRefPtr<IDBDatabase> database, int64_t transactionId, int64_t objectStoreId, const IDBIndexMetadata& indexMetadata)
        : EventListener(CPPEventListenerType)
        , m_database(database)
        , m_transactionId(transactionId)
        , m_objectStoreId(objectStoreId)
        , m_indexMetadata(indexMetadata)
    {
    }

    virtual void handleEvent(ExecutionContext* context, Event* event) OVERRIDE
    {
        ASSERT(event->type() == EventTypeNames::success);
        EventTarget* target = event->target();
        IDBRequest* request = static_cast<IDBRequest*>(target);

        RefPtr<IDBAny> cursorAny = request->resultAsAny();
        RefPtr<IDBCursorWithValue> cursor;
        if (cursorAny->type() == IDBAny::IDBCursorWithValueType)
            cursor = cursorAny->idbCursorWithValue();

        Vector<int64_t> indexIds;
        indexIds.append(m_indexMetadata.id);
        if (cursor) {
            cursor->continueFunction(static_cast<IDBKey*>(0), static_cast<IDBKey*>(0), ASSERT_NO_EXCEPTION);

            RefPtr<IDBKey> primaryKey = cursor->idbPrimaryKey();
            ScriptValue value = cursor->value(context);

            IDBObjectStore::IndexKeys indexKeys;
            generateIndexKeysForValue(request->requestState(), m_indexMetadata, value, &indexKeys);

            Vector<IDBObjectStore::IndexKeys> indexKeysList;
            indexKeysList.append(indexKeys);

            m_database->backend()->setIndexKeys(m_transactionId, m_objectStoreId, primaryKey.release(), indexIds, indexKeysList);
        } else {
            // Now that we are done indexing, tell the backend to go
            // back to processing tasks of type NormalTask.
            m_database->backend()->setIndexesReady(m_transactionId, m_objectStoreId, indexIds);
            m_database.clear();
        }

    }

    RefPtr<IDBDatabase> m_database;
    const int64_t m_transactionId;
    const int64_t m_objectStoreId;
    const IDBIndexMetadata m_indexMetadata;
};
}

PassRefPtr<IDBIndex> IDBObjectStore::createIndex(ExecutionContext* context, const String& name, const IDBKeyPath& keyPath, const Dictionary& options, ExceptionState& exceptionState)
{
    bool unique = false;
    options.get("unique", unique);

    bool multiEntry = false;
    options.get("multiEntry", multiEntry);

    return createIndex(context, name, keyPath, unique, multiEntry, exceptionState);
}

PassRefPtr<IDBIndex> IDBObjectStore::createIndex(ExecutionContext* context, const String& name, const IDBKeyPath& keyPath, bool unique, bool multiEntry, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::createIndex");
    if (!m_transaction->isVersionChange()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::notVersionChangeTransactionErrorMessage);
        return nullptr;
    }
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
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
    if (!keyPath.isValid()) {
        exceptionState.throwDOMException(SyntaxError, "The keyPath argument contains an invalid key path.");
        return nullptr;
    }
    if (name.isNull()) {
        exceptionState.throwTypeError("The name provided is null.");
        return nullptr;
    }
    if (containsIndex(name)) {
        exceptionState.throwDOMException(ConstraintError, "An index with the specified name already exists.");
        return nullptr;
    }

    if (keyPath.type() == IDBKeyPath::ArrayType && multiEntry) {
        exceptionState.throwDOMException(InvalidAccessError, "The keyPath argument was an array and the multiEntry option is true.");
        return nullptr;
    }

    int64_t indexId = m_metadata.maxIndexId + 1;
    backendDB()->createIndex(m_transaction->id(), id(), indexId, name, keyPath, unique, multiEntry);

    ++m_metadata.maxIndexId;

    IDBIndexMetadata metadata(name, indexId, keyPath, unique, multiEntry);
    RefPtr<IDBIndex> index = IDBIndex::create(metadata, this, m_transaction.get());
    m_indexMap.set(name, index);
    m_metadata.indexes.set(indexId, metadata);
    m_transaction->db()->indexCreated(id(), metadata);

    ASSERT(!exceptionState.hadException());
    if (exceptionState.hadException())
        return nullptr;

    RefPtr<IDBRequest> indexRequest = openCursor(context, static_cast<IDBKeyRange*>(0), blink::WebIDBCursor::Next, WebIDBDatabase::PreemptiveTask);
    indexRequest->preventPropagation();

    // This is kept alive by being the success handler of the request, which is in turn kept alive by the owning transaction.
    RefPtr<IndexPopulator> indexPopulator = IndexPopulator::create(transaction()->db(), m_transaction->id(), id(), metadata);
    indexRequest->setOnsuccess(indexPopulator);

    return index.release();
}

PassRefPtr<IDBIndex> IDBObjectStore::index(const String& name, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::index");
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
        return nullptr;
    }
    if (m_transaction->isFinished()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::transactionFinishedErrorMessage);
        return nullptr;
    }

    IDBIndexMap::iterator it = m_indexMap.find(name);
    if (it != m_indexMap.end())
        return it->value;

    int64_t indexId = findIndexId(name);
    if (indexId == IDBIndexMetadata::InvalidId) {
        exceptionState.throwDOMException(NotFoundError, IDBDatabase::noSuchIndexErrorMessage);
        return nullptr;
    }

    const IDBIndexMetadata* indexMetadata(0);
    for (IDBObjectStoreMetadata::IndexMap::const_iterator it = m_metadata.indexes.begin(); it != m_metadata.indexes.end(); ++it) {
        if (it->value.name == name) {
            indexMetadata = &it->value;
            break;
        }
    }
    ASSERT(indexMetadata);
    ASSERT(indexMetadata->id != IDBIndexMetadata::InvalidId);

    RefPtr<IDBIndex> index = IDBIndex::create(*indexMetadata, this, m_transaction.get());
    m_indexMap.set(name, index);
    return index.release();
}

void IDBObjectStore::deleteIndex(const String& name, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::deleteIndex");
    if (!m_transaction->isVersionChange()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::notVersionChangeTransactionErrorMessage);
        return;
    }
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
        return;
    }
    if (m_transaction->isFinished()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return;
    }
    if (!m_transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return;
    }
    int64_t indexId = findIndexId(name);
    if (indexId == IDBIndexMetadata::InvalidId) {
        exceptionState.throwDOMException(NotFoundError, IDBDatabase::noSuchIndexErrorMessage);
        return;
    }

    backendDB()->deleteIndex(m_transaction->id(), id(), indexId);

    m_metadata.indexes.remove(indexId);
    m_transaction->db()->indexDeleted(id(), indexId);
    IDBIndexMap::iterator it = m_indexMap.find(name);
    if (it != m_indexMap.end()) {
        it->value->markDeleted();
        m_indexMap.remove(name);
    }
}

PassRefPtr<IDBRequest> IDBObjectStore::openCursor(ExecutionContext* context, const ScriptValue& range, const String& directionString, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::openCursor");
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
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

    return openCursor(context, keyRange, direction, WebIDBDatabase::NormalTask);
}

PassRefPtr<IDBRequest> IDBObjectStore::openCursor(ExecutionContext* context, PassRefPtr<IDBKeyRange> range, WebIDBCursor::Direction direction, WebIDBDatabase::TaskType taskType)
{
    RefPtr<IDBRequest> request = IDBRequest::create(context, IDBAny::create(this), m_transaction.get());
    request->setCursorDetails(IndexedDB::CursorKeyAndValue, direction);

    backendDB()->openCursor(m_transaction->id(), id(), IDBIndexMetadata::InvalidId, range, direction, false, taskType, WebIDBCallbacksImpl::create(request).leakPtr());
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::openKeyCursor(ExecutionContext* context, const ScriptValue& range, const String& directionString, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::openKeyCursor");
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
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

    backendDB()->openCursor(m_transaction->id(), id(), IDBIndexMetadata::InvalidId, keyRange.release(), direction, true, WebIDBDatabase::NormalTask, WebIDBCallbacksImpl::create(request).leakPtr());
    return request.release();
}

PassRefPtr<IDBRequest> IDBObjectStore::count(ExecutionContext* context, const ScriptValue& range, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBObjectStore::count");
    if (isDeleted()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::objectStoreDeletedErrorMessage);
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
    backendDB()->count(m_transaction->id(), id(), IDBIndexMetadata::InvalidId, keyRange.release(), WebIDBCallbacksImpl::create(request).leakPtr());
    return request.release();
}

void IDBObjectStore::transactionFinished()
{
    ASSERT(m_transaction->isFinished());

    // Break reference cycles.
    m_indexMap.clear();
}

int64_t IDBObjectStore::findIndexId(const String& name) const
{
    for (IDBObjectStoreMetadata::IndexMap::const_iterator it = m_metadata.indexes.begin(); it != m_metadata.indexes.end(); ++it) {
        if (it->value.name == name) {
            ASSERT(it->key != IDBIndexMetadata::InvalidId);
            return it->key;
        }
    }
    return IDBIndexMetadata::InvalidId;
}

WebIDBDatabase* IDBObjectStore::backendDB() const
{
    return m_transaction->backendDB();
}

} // namespace WebCore
