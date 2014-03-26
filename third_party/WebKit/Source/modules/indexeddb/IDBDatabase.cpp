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
#include "modules/indexeddb/IDBDatabase.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/IDBBindingUtilities.h"
#include "bindings/v8/Nullable.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventQueue.h"
#include "core/inspector/ScriptCallStack.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBEventDispatcher.h"
#include "modules/indexeddb/IDBHistograms.h"
#include "modules/indexeddb/IDBIndex.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBVersionChangeEvent.h"
#include "modules/indexeddb/WebIDBDatabaseCallbacksImpl.h"
#include "public/platform/Platform.h"
#include "public/platform/WebIDBKeyPath.h"
#include "wtf/Atomics.h"
#include <limits>

using blink::WebIDBDatabase;

namespace WebCore {

const char IDBDatabase::indexDeletedErrorMessage[] = "The index or its object store has been deleted.";
const char IDBDatabase::isKeyCursorErrorMessage[] = "The cursor is a key cursor.";
const char IDBDatabase::noKeyOrKeyRangeErrorMessage[] = "No key or key range specified.";
const char IDBDatabase::noSuchIndexErrorMessage[] = "The specified index was not found.";
const char IDBDatabase::noSuchObjectStoreErrorMessage[] = "The specified object store was not found.";
const char IDBDatabase::noValueErrorMessage[] = "The cursor is being iterated or has iterated past its end.";
const char IDBDatabase::notValidKeyErrorMessage[] = "The parameter is not a valid key.";
const char IDBDatabase::notVersionChangeTransactionErrorMessage[] = "The database is not running a version change transaction.";
const char IDBDatabase::objectStoreDeletedErrorMessage[] = "The object store has been deleted.";
const char IDBDatabase::requestNotFinishedErrorMessage[] = "The request has not finished.";
const char IDBDatabase::sourceDeletedErrorMessage[] = "The cursor's source or effective object store has been deleted.";
const char IDBDatabase::transactionInactiveErrorMessage[] = "The transaction is not active.";
const char IDBDatabase::transactionFinishedErrorMessage[] = "The transaction has finished.";
const char IDBDatabase::transactionReadOnlyErrorMessage[] = "The transaction is read-only.";

PassRefPtr<IDBDatabase> IDBDatabase::create(ExecutionContext* context, PassOwnPtr<WebIDBDatabase> database, PassRefPtr<IDBDatabaseCallbacks> callbacks)
{
    RefPtr<IDBDatabase> idbDatabase(adoptRef(new IDBDatabase(context, database, callbacks)));
    idbDatabase->suspendIfNeeded();
    return idbDatabase.release();
}

IDBDatabase::IDBDatabase(ExecutionContext* context, PassOwnPtr<WebIDBDatabase> backend, PassRefPtr<IDBDatabaseCallbacks> callbacks)
    : ActiveDOMObject(context)
    , m_backend(backend)
    , m_closePending(false)
    , m_contextStopped(false)
    , m_databaseCallbacks(callbacks)
{
    // We pass a reference of this object before it can be adopted.
    relaxAdoptionRequirement();
    ScriptWrappable::init(this);
    m_databaseCallbacks->connect(this);
}

IDBDatabase::~IDBDatabase()
{
    close();
}

int64_t IDBDatabase::nextTransactionId()
{
    // Only keep a 32-bit counter to allow ports to use the other 32
    // bits of the id.
    AtomicallyInitializedStatic(int, currentTransactionId = 0);
    return atomicIncrement(&currentTransactionId);
}

void IDBDatabase::indexCreated(int64_t objectStoreId, const IDBIndexMetadata& metadata)
{
    IDBDatabaseMetadata::ObjectStoreMap::iterator it = m_metadata.objectStores.find(objectStoreId);
    ASSERT_WITH_SECURITY_IMPLICATION(it != m_metadata.objectStores.end());
    it->value.indexes.set(metadata.id, metadata);
}

void IDBDatabase::indexDeleted(int64_t objectStoreId, int64_t indexId)
{
    IDBDatabaseMetadata::ObjectStoreMap::iterator it = m_metadata.objectStores.find(objectStoreId);
    ASSERT_WITH_SECURITY_IMPLICATION(it != m_metadata.objectStores.end());
    it->value.indexes.remove(indexId);
}

void IDBDatabase::transactionCreated(IDBTransaction* transaction)
{
    ASSERT(transaction);
    ASSERT(!m_transactions.contains(transaction->id()));
    m_transactions.add(transaction->id(), transaction);

    if (transaction->isVersionChange()) {
        ASSERT(!m_versionChangeTransaction);
        m_versionChangeTransaction = transaction;
    }
}

void IDBDatabase::transactionFinished(const IDBTransaction* transaction)
{
    ASSERT(transaction);
    ASSERT(m_transactions.contains(transaction->id()));
    ASSERT(m_transactions.get(transaction->id()) == transaction);
    m_transactions.remove(transaction->id());

    if (transaction->isVersionChange()) {
        ASSERT(m_versionChangeTransaction == transaction);
        m_versionChangeTransaction = nullptr;
    }

    if (m_closePending && m_transactions.isEmpty())
        closeConnection();
}

void IDBDatabase::onAbort(int64_t transactionId, PassRefPtrWillBeRawPtr<DOMError> error)
{
    ASSERT(m_transactions.contains(transactionId));
    m_transactions.get(transactionId)->onAbort(error);
}

void IDBDatabase::onComplete(int64_t transactionId)
{
    ASSERT(m_transactions.contains(transactionId));
    m_transactions.get(transactionId)->onComplete();
}

PassRefPtr<DOMStringList> IDBDatabase::objectStoreNames() const
{
    RefPtr<DOMStringList> objectStoreNames = DOMStringList::create();
    for (IDBDatabaseMetadata::ObjectStoreMap::const_iterator it = m_metadata.objectStores.begin(); it != m_metadata.objectStores.end(); ++it)
        objectStoreNames->append(it->value.name);
    objectStoreNames->sort();
    return objectStoreNames.release();
}

ScriptValue IDBDatabase::version(ExecutionContext* context) const
{
    DOMRequestState requestState(context);
    int64_t intVersion = m_metadata.intVersion;
    if (intVersion == IDBDatabaseMetadata::NoIntVersion)
        return idbAnyToScriptValue(&requestState, IDBAny::createString(m_metadata.version));

    return idbAnyToScriptValue(&requestState, IDBAny::create(intVersion));
}

PassRefPtr<IDBObjectStore> IDBDatabase::createObjectStore(const String& name, const Dictionary& options, ExceptionState& exceptionState)
{
    IDBKeyPath keyPath;
    bool autoIncrement = false;
    if (!options.isUndefinedOrNull()) {
        String keyPathString;
        Vector<String> keyPathArray;
        if (options.get("keyPath", keyPathArray))
            keyPath = IDBKeyPath(keyPathArray);
        else if (options.getWithUndefinedOrNullCheck("keyPath", keyPathString))
            keyPath = IDBKeyPath(keyPathString);

        options.get("autoIncrement", autoIncrement);
    }

    return createObjectStore(name, keyPath, autoIncrement, exceptionState);
}

PassRefPtr<IDBObjectStore> IDBDatabase::createObjectStore(const String& name, const IDBKeyPath& keyPath, bool autoIncrement, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBDatabase::createObjectStore");
    blink::Platform::current()->histogramEnumeration("WebCore.IndexedDB.FrontEndAPICalls", IDBCreateObjectStoreCall, IDBMethodsMax);
    if (!m_versionChangeTransaction) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::notVersionChangeTransactionErrorMessage);
        return nullptr;
    }
    if (m_versionChangeTransaction->isFinished()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return nullptr;
    }
    if (!m_versionChangeTransaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return nullptr;
    }

    if (containsObjectStore(name)) {
        exceptionState.throwDOMException(ConstraintError, "An object store with the specified name already exists.");
        return nullptr;
    }

    if (!keyPath.isNull() && !keyPath.isValid()) {
        exceptionState.throwDOMException(SyntaxError, "The keyPath option is not a valid key path.");
        return nullptr;
    }

    if (autoIncrement && ((keyPath.type() == IDBKeyPath::StringType && keyPath.string().isEmpty()) || keyPath.type() == IDBKeyPath::ArrayType)) {
        exceptionState.throwDOMException(InvalidAccessError, "The autoIncrement option was set but the keyPath option was empty or an array.");
        return nullptr;
    }

    int64_t objectStoreId = m_metadata.maxObjectStoreId + 1;
    m_backend->createObjectStore(m_versionChangeTransaction->id(), objectStoreId, name, keyPath, autoIncrement);

    IDBObjectStoreMetadata metadata(name, objectStoreId, keyPath, autoIncrement, WebIDBDatabase::minimumIndexId);
    RefPtr<IDBObjectStore> objectStore = IDBObjectStore::create(metadata, m_versionChangeTransaction.get());
    m_metadata.objectStores.set(metadata.id, metadata);
    ++m_metadata.maxObjectStoreId;

    m_versionChangeTransaction->objectStoreCreated(name, objectStore);
    return objectStore.release();
}

void IDBDatabase::deleteObjectStore(const String& name, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBDatabase::deleteObjectStore");
    blink::Platform::current()->histogramEnumeration("WebCore.IndexedDB.FrontEndAPICalls", IDBDeleteObjectStoreCall, IDBMethodsMax);
    if (!m_versionChangeTransaction) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::notVersionChangeTransactionErrorMessage);
        return;
    }
    if (m_versionChangeTransaction->isFinished()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return;
    }
    if (!m_versionChangeTransaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return;
    }

    int64_t objectStoreId = findObjectStoreId(name);
    if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
        exceptionState.throwDOMException(NotFoundError, "The specified object store was not found.");
        return;
    }

    m_backend->deleteObjectStore(m_versionChangeTransaction->id(), objectStoreId);
    m_versionChangeTransaction->objectStoreDeleted(name);
    m_metadata.objectStores.remove(objectStoreId);
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ExecutionContext* context, const Vector<String>& scope, const String& modeString, ExceptionState& exceptionState)
{
    IDB_TRACE("IDBDatabase::transaction");
    blink::Platform::current()->histogramEnumeration("WebCore.IndexedDB.FrontEndAPICalls", IDBTransactionCall, IDBMethodsMax);
    if (!scope.size()) {
        exceptionState.throwDOMException(InvalidAccessError, "The storeNames parameter was empty.");
        return nullptr;
    }

    blink::WebIDBDatabase::TransactionMode mode = IDBTransaction::stringToMode(modeString, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    if (m_versionChangeTransaction) {
        exceptionState.throwDOMException(InvalidStateError, "A version change transaction is running.");
        return nullptr;
    }

    if (m_closePending) {
        exceptionState.throwDOMException(InvalidStateError, "The database connection is closing.");
        return nullptr;
    }

    Vector<int64_t> objectStoreIds;
    for (size_t i = 0; i < scope.size(); ++i) {
        int64_t objectStoreId = findObjectStoreId(scope[i]);
        if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
            exceptionState.throwDOMException(NotFoundError, "One of the specified object stores was not found.");
            return nullptr;
        }
        objectStoreIds.append(objectStoreId);
    }

    int64_t transactionId = nextTransactionId();
    m_backend->createTransaction(transactionId, WebIDBDatabaseCallbacksImpl::create(m_databaseCallbacks).leakPtr(), objectStoreIds, mode);

    RefPtr<IDBTransaction> transaction = IDBTransaction::create(context, transactionId, scope, mode, this);
    return transaction.release();
}

PassRefPtr<IDBTransaction> IDBDatabase::transaction(ExecutionContext* context, const String& storeName, const String& mode, ExceptionState& exceptionState)
{
    RefPtr<DOMStringList> storeNames = DOMStringList::create();
    storeNames->append(storeName);
    return transaction(context, storeNames, mode, exceptionState);
}

void IDBDatabase::forceClose()
{
    for (TransactionMap::const_iterator::Values it = m_transactions.begin().values(), end = m_transactions.end().values(); it != end; ++it)
        (*it)->abort(IGNORE_EXCEPTION);
    this->close();
    enqueueEvent(Event::create(EventTypeNames::close));
}

void IDBDatabase::close()
{
    IDB_TRACE("IDBDatabase::close");
    if (m_closePending)
        return;

    m_closePending = true;

    if (m_transactions.isEmpty())
        closeConnection();
}

void IDBDatabase::closeConnection()
{
    ASSERT(m_closePending);
    ASSERT(m_transactions.isEmpty());

    if (m_backend) {
        m_backend->close();
        m_backend.clear();
    }

    if (m_contextStopped || !executionContext())
        return;

    EventQueue* eventQueue = executionContext()->eventQueue();
    // Remove any pending versionchange events scheduled to fire on this
    // connection. They would have been scheduled by the backend when another
    // connection called setVersion, but the frontend connection is being
    // closed before they could fire.
    for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
        bool removed = eventQueue->cancelEvent(m_enqueuedEvents[i].get());
        ASSERT_UNUSED(removed, removed);
    }
}

void IDBDatabase::onVersionChange(int64_t oldVersion, int64_t newVersion)
{
    IDB_TRACE("IDBDatabase::onVersionChange");
    if (m_contextStopped || !executionContext())
        return;

    if (m_closePending)
        return;

    Nullable<unsigned long long> newVersionNullable = (newVersion == IDBDatabaseMetadata::NoIntVersion) ? Nullable<unsigned long long>() : Nullable<unsigned long long>(newVersion);
    enqueueEvent(IDBVersionChangeEvent::create(EventTypeNames::versionchange, oldVersion, newVersionNullable));
}

void IDBDatabase::enqueueEvent(PassRefPtr<Event> event)
{
    ASSERT(!m_contextStopped);
    ASSERT(executionContext());
    EventQueue* eventQueue = executionContext()->eventQueue();
    event->setTarget(this);
    eventQueue->enqueueEvent(event.get());
    m_enqueuedEvents.append(event);
}

bool IDBDatabase::dispatchEvent(PassRefPtr<Event> event)
{
    IDB_TRACE("IDBDatabase::dispatchEvent");
    if (m_contextStopped || !executionContext())
        return false;
    ASSERT(event->type() == EventTypeNames::versionchange || event->type() == EventTypeNames::close);
    for (size_t i = 0; i < m_enqueuedEvents.size(); ++i) {
        if (m_enqueuedEvents[i].get() == event.get())
            m_enqueuedEvents.remove(i);
    }
    return EventTarget::dispatchEvent(event.get());
}

int64_t IDBDatabase::findObjectStoreId(const String& name) const
{
    for (IDBDatabaseMetadata::ObjectStoreMap::const_iterator it = m_metadata.objectStores.begin(); it != m_metadata.objectStores.end(); ++it) {
        if (it->value.name == name) {
            ASSERT(it->key != IDBObjectStoreMetadata::InvalidId);
            return it->key;
        }
    }
    return IDBObjectStoreMetadata::InvalidId;
}

bool IDBDatabase::hasPendingActivity() const
{
    // The script wrapper must not be collected before the object is closed or
    // we can't fire a "versionchange" event to let script manually close the connection.
    return !m_closePending && hasEventListeners() && !m_contextStopped;
}

void IDBDatabase::stop()
{
    m_contextStopped = true;

    // Immediately close the connection to the back end. Don't attempt a
    // normal close() since that may wait on transactions which require a
    // round trip to the back-end to abort.
    if (m_backend) {
        m_backend->close();
        m_backend.clear();
    }
}

const AtomicString& IDBDatabase::interfaceName() const
{
    return EventTargetNames::IDBDatabase;
}

ExecutionContext* IDBDatabase::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

} // namespace WebCore
