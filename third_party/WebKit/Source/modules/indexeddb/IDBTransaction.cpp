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

#include "modules/indexeddb/IDBTransaction.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventQueue.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBEventDispatcher.h"
#include "modules/indexeddb/IDBIndex.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "modules/indexeddb/IDBTracing.h"
#include "wtf/PtrUtil.h"
#include <memory>

using blink::WebIDBDatabase;

namespace blink {

IDBTransaction* IDBTransaction::createNonVersionChange(ScriptState* scriptState, int64_t id, const HashSet<String>& scope, WebIDBTransactionMode mode, IDBDatabase* db)
{
    DCHECK_NE(mode,  WebIDBTransactionModeVersionChange);
    DCHECK(!scope.isEmpty()) << "Non-version transactions should operate on a well-defined set of stores";
    IDBOpenDBRequest* openDBRequest = nullptr;
    IDBTransaction* transaction = new IDBTransaction(scriptState, id, scope, mode, db, openDBRequest, IDBDatabaseMetadata());
    transaction->suspendIfNeeded();
    return transaction;
}

IDBTransaction* IDBTransaction::createVersionChange(ScriptState* scriptState, int64_t id, IDBDatabase* db, IDBOpenDBRequest* openDBRequest, const IDBDatabaseMetadata& oldMetadata)
{
    IDBTransaction* transaction = new IDBTransaction(scriptState, id, HashSet<String>(), WebIDBTransactionModeVersionChange, db, openDBRequest, oldMetadata);
    transaction->suspendIfNeeded();
    return transaction;
}

namespace {

class DeactivateTransactionTask : public V8PerIsolateData::EndOfScopeTask {
public:
    static std::unique_ptr<DeactivateTransactionTask> create(IDBTransaction* transaction)
    {
        return wrapUnique(new DeactivateTransactionTask(transaction));
    }

    void run() override
    {
        m_transaction->setActive(false);
        m_transaction.clear();
    }

private:
    explicit DeactivateTransactionTask(IDBTransaction* transaction)
        : m_transaction(transaction) { }

    Persistent<IDBTransaction> m_transaction;
};

} // namespace

IDBTransaction::IDBTransaction(ScriptState* scriptState, int64_t id, const HashSet<String>& scope, WebIDBTransactionMode mode, IDBDatabase* db, IDBOpenDBRequest* openDBRequest, const IDBDatabaseMetadata& oldMetadata)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(scriptState->getExecutionContext())
    , m_id(id)
    , m_database(db)
    , m_openDBRequest(openDBRequest)
    , m_mode(mode)
    , m_scope(scope)
    , m_oldDatabaseMetadata(oldMetadata)
{
    DCHECK(m_database);

    if (isVersionChange()) {
        DCHECK(scope.isEmpty());

        // Not active until the callback.
        m_state = Inactive;
    } else {
        DCHECK(!scope.isEmpty()) << "Non-versionchange transactions must operate on a well-defined set of stores";
        DCHECK(m_mode == WebIDBTransactionModeReadOnly || m_mode == WebIDBTransactionModeReadWrite) << "Invalid transaction mode";
    }

    if (m_state == Active)
        V8PerIsolateData::from(scriptState->isolate())->addEndOfScopeTask(DeactivateTransactionTask::create(this));
    m_database->transactionCreated(this);
}

IDBTransaction::~IDBTransaction()
{
    DCHECK(m_state == Finished || m_contextStopped);
    DCHECK(m_requestList.isEmpty() || m_contextStopped);
}

DEFINE_TRACE(IDBTransaction)
{
    visitor->trace(m_database);
    visitor->trace(m_openDBRequest);
    visitor->trace(m_error);
    visitor->trace(m_requestList);
    visitor->trace(m_objectStoreMap);
    visitor->trace(m_createdObjectStores);
    visitor->trace(m_deletedObjectStores);
    visitor->trace(m_objectStoreCleanupMap);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

void IDBTransaction::setError(DOMException* error)
{
    DCHECK_NE(m_state, Finished);
    DCHECK(error);

    // The first error to be set is the true cause of the
    // transaction abort.
    if (!m_error) {
        m_error = error;
    }
}

IDBObjectStore* IDBTransaction::objectStore(const String& name, ExceptionState& exceptionState)
{
    if (isFinished()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::transactionFinishedErrorMessage);
        return nullptr;
    }

    IDBObjectStoreMap::iterator it = m_objectStoreMap.find(name);
    if (it != m_objectStoreMap.end())
        return it->value;

    if (!isVersionChange() && !m_scope.contains(name)) {
        exceptionState.throwDOMException(NotFoundError, IDBDatabase::noSuchObjectStoreErrorMessage);
        return nullptr;
    }

    int64_t objectStoreId = m_database->findObjectStoreId(name);
    if (objectStoreId == IDBObjectStoreMetadata::InvalidId) {
        DCHECK(isVersionChange());
        exceptionState.throwDOMException(NotFoundError, IDBDatabase::noSuchObjectStoreErrorMessage);
        return nullptr;
    }

    DCHECK(m_database->metadata().objectStores.contains(objectStoreId));
    const IDBObjectStoreMetadata& objectStoreMetadata = m_database->metadata().objectStores.get(objectStoreId);

    IDBObjectStore* objectStore = IDBObjectStore::create(objectStoreMetadata, this);
    DCHECK(!m_objectStoreMap.contains(name));
    m_objectStoreMap.set(name, objectStore);
    m_objectStoreCleanupMap.set(objectStore, objectStore->metadata());
    return objectStore;
}

void IDBTransaction::objectStoreCreated(const String& name, IDBObjectStore* objectStore)
{
    DCHECK_NE(m_state, Finished) << "A finished transaction created an object store";
    DCHECK_EQ(m_mode, WebIDBTransactionModeVersionChange) << "A non-versionchange transaction created an object store";
    DCHECK(!m_objectStoreMap.contains(name)) << "An object store was created with the name of an existing store";
    m_objectStoreMap.set(name, objectStore);
    m_objectStoreCleanupMap.set(objectStore, objectStore->metadata());
    m_createdObjectStores.add(objectStore);
}

void IDBTransaction::objectStoreDeleted(const String& name)
{
    DCHECK_NE(m_state, Finished) << "A finished transaction deleted an object store";
    DCHECK_EQ(m_mode, WebIDBTransactionModeVersionChange) << "A non-versionchange transaction deleted an object store";
    IDBObjectStoreMap::iterator it = m_objectStoreMap.find(name);
    if (it != m_objectStoreMap.end()) {
        IDBObjectStore* objectStore = it->value;
        m_objectStoreMap.remove(name);
        objectStore->markDeleted();
        m_objectStoreCleanupMap.set(objectStore, objectStore->metadata());
        m_deletedObjectStores.add(objectStore);
    }
}

void IDBTransaction::objectStoreRenamed(const String& oldName, const String& newName)
{
    DCHECK_NE(m_state, Finished) << "A finished transaction renamed an object store";
    DCHECK_EQ(m_mode, WebIDBTransactionModeVersionChange) << "A non-versionchange transaction renamed an object store";

    DCHECK(!m_objectStoreMap.contains(newName));
    DCHECK(m_objectStoreMap.contains(oldName)) << "The object store had to be accessed in order to be renamed.";
    m_objectStoreMap.set(newName, m_objectStoreMap.take(oldName));
}

void IDBTransaction::setActive(bool active)
{
    DCHECK_NE(m_state, Finished) << "A finished transaction tried to setActive(" << (active ? "true" : "false") << ")";
    if (m_state == Finishing)
        return;
    DCHECK_NE(active, (m_state == Active));
    m_state = active ? Active : Inactive;

    if (!active && m_requestList.isEmpty() && backendDB())
        backendDB()->commit(m_id);
}

void IDBTransaction::abort(ExceptionState& exceptionState)
{
    if (m_state == Finishing || m_state == Finished) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::transactionFinishedErrorMessage);
        return;
    }

    m_state = Finishing;

    if (m_contextStopped)
        return;

    abortOutstandingRequests();
    revertDatabaseMetadata();

    if (backendDB())
        backendDB()->abort(m_id);
}

void IDBTransaction::registerRequest(IDBRequest* request)
{
    DCHECK(request);
    DCHECK_EQ(m_state, Active);
    m_requestList.add(request);
}

void IDBTransaction::unregisterRequest(IDBRequest* request)
{
    DCHECK(request);
    // If we aborted the request, it will already have been removed.
    m_requestList.remove(request);
}

void IDBTransaction::onAbort(DOMException* error)
{
    IDB_TRACE("IDBTransaction::onAbort");
    if (m_contextStopped) {
        finished();
        return;
    }

    DCHECK_NE(m_state, Finished);
    if (m_state != Finishing) {
        // Abort was not triggered by front-end.
        DCHECK(error);
        setError(error);

        abortOutstandingRequests();
        revertDatabaseMetadata();

        m_state = Finishing;
    }

    if (isVersionChange())
        m_database->close();

    // Enqueue events before notifying database, as database may close which enqueues more events and order matters.
    enqueueEvent(Event::createBubble(EventTypeNames::abort));
    finished();
}

void IDBTransaction::onComplete()
{
    IDB_TRACE("IDBTransaction::onComplete");
    if (m_contextStopped) {
        finished();
        return;
    }

    DCHECK_NE(m_state, Finished);
    m_state = Finishing;

    // Enqueue events before notifying database, as database may close which enqueues more events and order matters.
    enqueueEvent(Event::create(EventTypeNames::complete));
    finished();
}

bool IDBTransaction::hasPendingActivity() const
{
    // FIXME: In an ideal world, we should return true as long as anyone has a or can
    //        get a handle to us or any child request object and any of those have
    //        event listeners. This is  in order to handle user generated events properly.
    return m_hasPendingActivity && !m_contextStopped;
}

WebIDBTransactionMode IDBTransaction::stringToMode(const String& modeString)
{
    if (modeString == IndexedDBNames::readonly)
        return WebIDBTransactionModeReadOnly;
    if (modeString == IndexedDBNames::readwrite)
        return WebIDBTransactionModeReadWrite;
    if (modeString == IndexedDBNames::versionchange)
        return WebIDBTransactionModeVersionChange;
    NOTREACHED();
    return WebIDBTransactionModeReadOnly;
}

const String& IDBTransaction::mode() const
{
    switch (m_mode) {
    case WebIDBTransactionModeReadOnly:
        return IndexedDBNames::readonly;

    case WebIDBTransactionModeReadWrite:
        return IndexedDBNames::readwrite;

    case WebIDBTransactionModeVersionChange:
        return IndexedDBNames::versionchange;
    }

    NOTREACHED();
    return IndexedDBNames::readonly;
}

WebIDBDatabase* IDBTransaction::backendDB() const
{
    return m_database->backend();
}

DOMStringList* IDBTransaction::objectStoreNames() const
{
    if (isVersionChange())
        return m_database->objectStoreNames();

    DOMStringList* objectStoreNames = DOMStringList::create(DOMStringList::IndexedDB);
    for (const String& objectStoreName : m_scope)
        objectStoreNames->append(objectStoreName);
    objectStoreNames->sort();
    return objectStoreNames;
}

const AtomicString& IDBTransaction::interfaceName() const
{
    return EventTargetNames::IDBTransaction;
}

ExecutionContext* IDBTransaction::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

DispatchEventResult IDBTransaction::dispatchEventInternal(Event* event)
{
    IDB_TRACE("IDBTransaction::dispatchEvent");
    if (m_contextStopped || !getExecutionContext()) {
        m_state = Finished;
        return DispatchEventResult::CanceledBeforeDispatch;
    }
    DCHECK_NE(m_state, Finished);
    DCHECK(m_hasPendingActivity);
    DCHECK(getExecutionContext());
    DCHECK_EQ(event->target(), this);
    m_state = Finished;

    HeapVector<Member<EventTarget>> targets;
    targets.append(this);
    targets.append(db());

    // FIXME: When we allow custom event dispatching, this will probably need to change.
    DCHECK(event->type() == EventTypeNames::complete || event->type() == EventTypeNames::abort);
    DispatchEventResult dispatchResult = IDBEventDispatcher::dispatch(event, targets);
    // FIXME: Try to construct a test where |this| outlives openDBRequest and we
    // get a crash.
    if (m_openDBRequest) {
        DCHECK(isVersionChange());
        m_openDBRequest->transactionDidFinishAndDispatch();
    }
    m_hasPendingActivity = false;
    return dispatchResult;
}

void IDBTransaction::stop()
{
    if (m_contextStopped)
        return;

    m_contextStopped = true;

    abort(IGNORE_EXCEPTION);
}

void IDBTransaction::enqueueEvent(Event* event)
{
    DCHECK_NE(m_state, Finished) << "A finished transaction tried to enqueue an event of type " << event->type() << ".";
    if (m_contextStopped || !getExecutionContext())
        return;

    EventQueue* eventQueue = getExecutionContext()->getEventQueue();
    event->setTarget(this);
    eventQueue->enqueueEvent(event);
}

void IDBTransaction::abortOutstandingRequests()
{
    for (IDBRequest* request : m_requestList)
        request->abort();
    m_requestList.clear();
}

void IDBTransaction::revertDatabaseMetadata()
{
    DCHECK_NE(m_state, Active);
    if (!isVersionChange())
        return;

    // Newly created stores must be marked as deleted.
    for (IDBObjectStore* store : m_createdObjectStores) {
        store->abort();
        store->markDeleted();
    }

    // Used stores may need to mark indexes as deleted.
    for (auto& it : m_objectStoreCleanupMap) {
        it.key->abort();
        it.key->setMetadata(it.value);
    }

    m_database->setMetadata(m_oldDatabaseMetadata);
}

void IDBTransaction::finished()
{
#if DCHECK_IS_ON()
    DCHECK(!m_finishCalled);
    m_finishCalled = true;
#endif // DCHECK_IS_ON()

    m_database->transactionFinished(this);

    // Break reference cycles.
    // TODO(jsbell): This can be removed c/o Oilpan.
    for (auto& it : m_objectStoreMap)
        it.value->transactionFinished();
    m_objectStoreMap.clear();
    for (auto& it : m_deletedObjectStores)
        it->transactionFinished();
    m_createdObjectStores.clear();
    m_deletedObjectStores.clear();

    m_objectStoreCleanupMap.clear();
}

} // namespace blink
