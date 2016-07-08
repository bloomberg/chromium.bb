// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "core/dom/ExceptionCode.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBObserverCallback.h"
#include "modules/indexeddb/IDBObserverInit.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/WebIDBObserverImpl.h"

namespace blink {

IDBObserver* IDBObserver::create(IDBObserverCallback& callback, const IDBObserverInit& options)
{
    return new IDBObserver(callback, options);
}

IDBObserver::IDBObserver(IDBObserverCallback& callback, const IDBObserverInit& options)
    : m_callback(&callback)
    , m_transaction(options.transaction())
    , m_values(options.values())
    , m_noRecords(options.noRecords())
{
}

IDBObserver::~IDBObserver() {}

void IDBObserver::observe(IDBDatabase* database, IDBTransaction* transaction, ExceptionState& exceptionState)
{
    if (transaction->isFinished() || transaction->isFinishing()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionFinishedErrorMessage);
        return;
    }
    if (!transaction->isActive()) {
        exceptionState.throwDOMException(TransactionInactiveError, IDBDatabase::transactionInactiveErrorMessage);
        return;
    }
    if (!database->backend()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::databaseClosedErrorMessage);
        return;
    }

    std::unique_ptr<WebIDBObserverImpl> observer = WebIDBObserverImpl::create(this);
    WebIDBObserverImpl* observerPtr = observer.get();
    int32_t observerId = database->backend()->addObserver(std::move(observer), transaction->id());
    m_observerIds.insert(observerId);
    observerPtr->setId(observerId);
}

void IDBObserver::unobserve(IDBDatabase* database, ExceptionState& exceptionState)
{
    if (!database->backend()) {
        exceptionState.throwDOMException(InvalidStateError, IDBDatabase::databaseClosedErrorMessage);
        return;
    }
    auto it = m_observerIds.begin();
    std::vector<int32_t> observerIdsToRemove;
    while (it != m_observerIds.end()) {
        if (database->backend()->containsObserverId(*it)) {
            observerIdsToRemove.push_back(*it);
            it = m_observerIds.erase(it);
        } else {
            it++;
        }
    }
    if (!observerIdsToRemove.empty())
        database->backend()->removeObservers(observerIdsToRemove);
}

void IDBObserver::removeObserver(int32_t id)
{
    m_observerIds.erase(id);
}

DEFINE_TRACE(IDBObserver)
{
    visitor->trace(m_callback);
}

} // namespace blink
