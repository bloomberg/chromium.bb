// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBObserver.h"

#include <bitset>

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/IDBObserverCallback.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "core/dom/ExceptionCode.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBObserverChanges.h"
#include "modules/indexeddb/IDBObserverInit.h"
#include "modules/indexeddb/IDBTransaction.h"

namespace blink {

IDBObserver* IDBObserver::Create(IDBObserverCallback* callback) {
  return new IDBObserver(callback);
}

IDBObserver::IDBObserver(IDBObserverCallback* callback) : callback_(callback) {}

void IDBObserver::observe(IDBDatabase* database,
                          IDBTransaction* transaction,
                          const IDBObserverInit& options,
                          ExceptionState& exception_state) {
  if (!transaction->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction->InactiveErrorMessage());
    return;
  }
  if (transaction->IsVersionChange()) {
    exception_state.ThrowDOMException(
        kTransactionInactiveError,
        IDBDatabase::kCannotObserveVersionChangeTransaction);
    return;
  }
  if (!database->Backend()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return;
  }
  if (!options.hasOperationTypes()) {
    exception_state.ThrowTypeError(
        "operationTypes not specified in observe options.");
    return;
  }
  if (options.operationTypes().IsEmpty()) {
    exception_state.ThrowTypeError("operationTypes must be populated.");
    return;
  }

  std::bitset<kWebIDBOperationTypeCount> types;
  for (const auto& operation_type : options.operationTypes()) {
    if (operation_type == IndexedDBNames::add) {
      types[kWebIDBAdd] = true;
    } else if (operation_type == IndexedDBNames::put) {
      types[kWebIDBPut] = true;
    } else if (operation_type == IndexedDBNames::kDelete) {
      types[kWebIDBDelete] = true;
    } else if (operation_type == IndexedDBNames::clear) {
      types[kWebIDBClear] = true;
    } else {
      exception_state.ThrowTypeError(
          "Unknown operation type in observe options: " + operation_type);
      return;
    }
  }

  int32_t observer_id =
      database->AddObserver(this, transaction->Id(), options.transaction(),
                            options.noRecords(), options.values(), types);
  observer_ids_.insert(observer_id, database);
}

void IDBObserver::unobserve(IDBDatabase* database,
                            ExceptionState& exception_state) {
  if (!database->Backend()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return;
  }

  Vector<int32_t> observer_ids_to_remove;
  for (const auto& it : observer_ids_) {
    if (it.value == database)
      observer_ids_to_remove.push_back(it.key);
  }
  observer_ids_.RemoveAll(observer_ids_to_remove);

  if (!observer_ids_to_remove.IsEmpty())
    database->RemoveObservers(observer_ids_to_remove);
}

DEFINE_TRACE(IDBObserver) {
  visitor->Trace(callback_);
  visitor->Trace(observer_ids_);
}

DEFINE_TRACE_WRAPPERS(IDBObserver) {
  visitor->TraceWrappers(callback_);
  ScriptWrappable::TraceWrappers(visitor);
}

}  // namespace blink
