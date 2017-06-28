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

#include "modules/indexeddb/IDBIndex.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "public/platform/modules/indexeddb/WebIDBKeyRange.h"

using blink::WebIDBCallbacks;
using blink::WebIDBCursor;
using blink::WebIDBDatabase;

namespace blink {

IDBIndex::IDBIndex(RefPtr<IDBIndexMetadata> metadata,
                   IDBObjectStore* object_store,
                   IDBTransaction* transaction)
    : metadata_(std::move(metadata)),
      object_store_(object_store),
      transaction_(transaction) {
  DCHECK(object_store_);
  DCHECK(transaction_);
  DCHECK(metadata_.Get());
  DCHECK_NE(Id(), IDBIndexMetadata::kInvalidId);
}

IDBIndex::~IDBIndex() {}

DEFINE_TRACE(IDBIndex) {
  visitor->Trace(object_store_);
  visitor->Trace(transaction_);
}

void IDBIndex::setName(const String& name, ExceptionState& exception_state) {
  IDB_TRACE("IDBIndex::setName");
  if (!transaction_->IsVersionChange()) {
    exception_state.ThrowDOMException(
        kInvalidStateError,
        IDBDatabase::kNotVersionChangeTransactionErrorMessage);
    return;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return;
  }

  if (this->name() == name)
    return;
  if (object_store_->ContainsIndex(name)) {
    exception_state.ThrowDOMException(kConstraintError,
                                      IDBDatabase::kIndexNameTakenErrorMessage);
    return;
  }
  if (!BackendDB()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return;
  }

  object_store_->RenameIndex(Id(), name);
}

ScriptValue IDBIndex::keyPath(ScriptState* script_state) const {
  return ScriptValue::From(script_state, Metadata().key_path);
}

void IDBIndex::RevertMetadata(RefPtr<IDBIndexMetadata> old_metadata) {
  metadata_ = std::move(old_metadata);

  // An index's metadata will only get reverted if the index was in the
  // database when the versionchange transaction started.
  deleted_ = false;
}

IDBRequest* IDBIndex::openCursor(ScriptState* script_state,
                                 const ScriptValue& range,
                                 const String& direction_string,
                                 ExceptionState& exception_state) {
  IDB_TRACE1("IDBIndex::openCursorRequestSetup", "index_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBIndex::openCursor", this,
                                      ++next_tracing_id_);
  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return nullptr;
  }
  WebIDBCursorDirection direction =
      IDBCursor::StringToDirection(direction_string);
  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (!BackendDB()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  return openCursor(script_state, key_range, direction, std::move(metrics));
}

IDBRequest* IDBIndex::openCursor(ScriptState* script_state,
                                 IDBKeyRange* key_range,
                                 WebIDBCursorDirection direction,
                                 IDBRequest::AsyncTraceState metrics) {
  IDBRequest* request =
      IDBRequest::Create(script_state, IDBAny::Create(this), transaction_.Get(),
                         std::move(metrics));
  request->SetCursorDetails(IndexedDB::kCursorKeyAndValue, direction);
  BackendDB()->OpenCursor(transaction_->Id(), object_store_->Id(), Id(),
                          key_range, direction, false, kWebIDBTaskTypeNormal,
                          request->CreateWebCallbacks().release());
  return request;
}

IDBRequest* IDBIndex::count(ScriptState* script_state,
                            const ScriptValue& range,
                            ExceptionState& exception_state) {
  IDB_TRACE1("IDBIndex::countRequestSetup", "index_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBIndex::count", this,
                                      ++next_tracing_id_);
  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return nullptr;
  }

  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (!BackendDB()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request =
      IDBRequest::Create(script_state, IDBAny::Create(this), transaction_.Get(),
                         std::move(metrics));
  BackendDB()->Count(transaction_->Id(), object_store_->Id(), Id(), key_range,
                     request->CreateWebCallbacks().release());
  return request;
}

IDBRequest* IDBIndex::openKeyCursor(ScriptState* script_state,
                                    const ScriptValue& range,
                                    const String& direction_string,
                                    ExceptionState& exception_state) {
  IDB_TRACE1("IDBIndex::openKeyCursorRequestSetup", "index_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBIndex::openKeyCursor", this,
                                      ++next_tracing_id_);
  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return nullptr;
  }
  WebIDBCursorDirection direction =
      IDBCursor::StringToDirection(direction_string);
  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!BackendDB()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request =
      IDBRequest::Create(script_state, IDBAny::Create(this), transaction_.Get(),
                         std::move(metrics));
  request->SetCursorDetails(IndexedDB::kCursorKeyOnly, direction);
  BackendDB()->OpenCursor(transaction_->Id(), object_store_->Id(), Id(),
                          key_range, direction, true, kWebIDBTaskTypeNormal,
                          request->CreateWebCallbacks().release());
  return request;
}

IDBRequest* IDBIndex::get(ScriptState* script_state,
                          const ScriptValue& key,
                          ExceptionState& exception_state) {
  IDB_TRACE1("IDBIndex::getRequestSetup", "index_name", metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBIndex::get", this,
                                      ++next_tracing_id_);
  return GetInternal(script_state, key, exception_state, false,
                     std::move(metrics));
}

IDBRequest* IDBIndex::getAll(ScriptState* script_state,
                             const ScriptValue& range,
                             ExceptionState& exception_state) {
  return getAll(script_state, range, std::numeric_limits<uint32_t>::max(),
                exception_state);
}

IDBRequest* IDBIndex::getAll(ScriptState* script_state,
                             const ScriptValue& range,
                             unsigned long max_count,
                             ExceptionState& exception_state) {
  IDB_TRACE1("IDBIndex::getAllRequestSetup", "index_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBIndex::getAll", this,
                                      ++next_tracing_id_);
  return GetAllInternal(script_state, range, max_count, exception_state, false,
                        std::move(metrics));
}

IDBRequest* IDBIndex::getAllKeys(ScriptState* script_state,
                                 const ScriptValue& range,
                                 ExceptionState& exception_state) {
  return getAllKeys(script_state, range, std::numeric_limits<uint32_t>::max(),
                    exception_state);
}

IDBRequest* IDBIndex::getAllKeys(ScriptState* script_state,
                                 const ScriptValue& range,
                                 uint32_t max_count,
                                 ExceptionState& exception_state) {
  IDB_TRACE1("IDBIndex::getAllKeysRequestSetup", "index_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBIndex::getAllKeys", this,
                                      ++next_tracing_id_);
  return GetAllInternal(script_state, range, max_count, exception_state,
                        /*key_only=*/true, std::move(metrics));
}

IDBRequest* IDBIndex::getKey(ScriptState* script_state,
                             const ScriptValue& key,
                             ExceptionState& exception_state) {
  IDB_TRACE1("IDBIndex::getKeyRequestSetup", "index_name",
             metadata_->name.Utf8());
  IDBRequest::AsyncTraceState metrics("IDBIndex::getKey", this,
                                      ++next_tracing_id_);
  return GetInternal(script_state, key, exception_state, true,
                     std::move(metrics));
}

IDBRequest* IDBIndex::GetInternal(ScriptState* script_state,
                                  const ScriptValue& key,
                                  ExceptionState& exception_state,
                                  bool key_only,
                                  IDBRequest::AsyncTraceState metrics) {
  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return nullptr;
  }

  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), key, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!key_range) {
    exception_state.ThrowDOMException(
        kDataError, IDBDatabase::kNoKeyOrKeyRangeErrorMessage);
    return nullptr;
  }
  if (!BackendDB()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request =
      IDBRequest::Create(script_state, IDBAny::Create(this), transaction_.Get(),
                         std::move(metrics));
  BackendDB()->Get(transaction_->Id(), object_store_->Id(), Id(), key_range,
                   key_only, request->CreateWebCallbacks().release());
  return request;
}

IDBRequest* IDBIndex::GetAllInternal(ScriptState* script_state,
                                     const ScriptValue& range,
                                     unsigned long max_count,
                                     ExceptionState& exception_state,
                                     bool key_only,
                                     IDBRequest::AsyncTraceState metrics) {
  if (!max_count)
    max_count = std::numeric_limits<uint32_t>::max();

  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kIndexDeletedErrorMessage);
    return nullptr;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return nullptr;
  }

  IDBKeyRange* key_range = IDBKeyRange::FromScriptValue(
      ExecutionContext::From(script_state), range, exception_state);
  if (exception_state.HadException())
    return nullptr;
  if (!BackendDB()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request =
      IDBRequest::Create(script_state, IDBAny::Create(this), transaction_.Get(),
                         std::move(metrics));
  BackendDB()->GetAll(transaction_->Id(), object_store_->Id(), Id(), key_range,
                      max_count, key_only,
                      request->CreateWebCallbacks().release());
  return request;
}

WebIDBDatabase* IDBIndex::BackendDB() const {
  return transaction_->BackendDB();
}

}  // namespace blink
