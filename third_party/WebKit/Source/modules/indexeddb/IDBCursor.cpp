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

#include "modules/indexeddb/IDBCursor.h"

#include <limits>
#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "bindings/modules/v8/V8IDBRequest.h"
#include "core/dom/ExceptionCode.h"
#include "modules/indexed_db_names.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBKeyRange.h"

using blink::WebIDBCursor;
using blink::WebIDBDatabase;

namespace blink {

IDBCursor* IDBCursor::Create(std::unique_ptr<WebIDBCursor> backend,
                             WebIDBCursorDirection direction,
                             IDBRequest* request,
                             const Source& source,
                             IDBTransaction* transaction) {
  return new IDBCursor(std::move(backend), direction, request, source,
                       transaction);
}

IDBCursor::IDBCursor(std::unique_ptr<WebIDBCursor> backend,
                     WebIDBCursorDirection direction,
                     IDBRequest* request,
                     const Source& source,
                     IDBTransaction* transaction)
    : backend_(std::move(backend)),
      request_(request),
      direction_(direction),
      source_(source),
      transaction_(transaction) {
  DCHECK(backend_);
  DCHECK(request_);
  DCHECK(!source_.IsNull());
  DCHECK(transaction_);
}

IDBCursor::~IDBCursor() = default;

void IDBCursor::Trace(blink::Visitor* visitor) {
  visitor->Trace(request_);
  visitor->Trace(source_);
  visitor->Trace(transaction_);
  visitor->Trace(value_);
  ScriptWrappable::Trace(visitor);
}

// Keep the request's wrapper alive as long as the cursor's wrapper is alive,
// so that the same script object is seen each time the cursor is used.
v8::Local<v8::Object> IDBCursor::AssociateWithWrapper(
    v8::Isolate* isolate,
    const WrapperTypeInfo* wrapper_type,
    v8::Local<v8::Object> wrapper) {
  wrapper =
      ScriptWrappable::AssociateWithWrapper(isolate, wrapper_type, wrapper);
  if (!wrapper.IsEmpty()) {
    V8PrivateProperty::GetIDBCursorRequest(isolate).Set(
        wrapper, ToV8(request_.Get(), wrapper, isolate));
  }
  return wrapper;
}

IDBRequest* IDBCursor::update(ScriptState* script_state,
                              const ScriptValue& value,
                              ExceptionState& exception_state) {
  IDB_TRACE("IDBCursor::updateRequestSetup");
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return nullptr;
  }
  if (transaction_->IsReadOnly()) {
    exception_state.ThrowDOMException(
        kReadOnlyError,
        "The record may not be updated inside a read-only transaction.");
    return nullptr;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return nullptr;
  }
  if (!got_value_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return nullptr;
  }
  if (IsKeyCursor()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kIsKeyCursorErrorMessage);
    return nullptr;
  }

  IDBObjectStore* object_store = EffectiveObjectStore();
  return object_store->DoPut(script_state, kWebIDBPutModeCursorUpdate,
                             IDBRequest::Source::FromIDBCursor(this), value,
                             IdbPrimaryKey(), exception_state);
}

void IDBCursor::advance(unsigned count, ExceptionState& exception_state) {
  IDB_TRACE("IDBCursor::advanceRequestSetup");
  IDBRequest::AsyncTraceState metrics("IDBCursor::advance");
  if (!count) {
    exception_state.ThrowTypeError(
        "A count argument with value 0 (zero) was supplied, must be greater "
        "than 0.");
    return;
  }
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return;
  }
  if (!got_value_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return;
  }

  request_->SetPendingCursor(this);
  request_->AssignNewMetrics(std::move(metrics));
  got_value_ = false;
  backend_->Advance(count, request_->CreateWebCallbacks().release());
}

void IDBCursor::Continue(ScriptState* script_state,
                         const ScriptValue& key_value,
                         ExceptionState& exception_state) {
  IDB_TRACE("IDBCursor::continueRequestSetup");
  IDBRequest::AsyncTraceState metrics("IDBCursor::continue");

  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return;
  }
  if (!got_value_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return;
  }

  std::unique_ptr<IDBKey> key =
      key_value.IsUndefined() || key_value.IsNull()
          ? nullptr
          : ScriptValue::To<std::unique_ptr<IDBKey>>(
                script_state->GetIsolate(), key_value, exception_state);
  if (exception_state.HadException())
    return;
  if (key && !key->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return;
  }
  Continue(std::move(key), nullptr, std::move(metrics), exception_state);
}

void IDBCursor::continuePrimaryKey(ScriptState* script_state,
                                   const ScriptValue& key_value,
                                   const ScriptValue& primary_key_value,
                                   ExceptionState& exception_state) {
  IDB_TRACE("IDBCursor::continuePrimaryKeyRequestSetup");
  IDBRequest::AsyncTraceState metrics("IDBCursor::continuePrimaryKey");

  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return;
  }

  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return;
  }

  if (!source_.IsIDBIndex()) {
    exception_state.ThrowDOMException(kInvalidAccessError,
                                      "The cursor's source is not an index.");
    return;
  }

  if (direction_ != kWebIDBCursorDirectionNext &&
      direction_ != kWebIDBCursorDirectionPrev) {
    exception_state.ThrowDOMException(
        kInvalidAccessError, "The cursor's direction is not 'next' or 'prev'.");
    return;
  }

  if (!got_value_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return;
  }

  std::unique_ptr<IDBKey> key = ScriptValue::To<std::unique_ptr<IDBKey>>(
      script_state->GetIsolate(), key_value, exception_state);
  if (exception_state.HadException())
    return;
  if (!key->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return;
  }

  std::unique_ptr<IDBKey> primary_key =
      ScriptValue::To<std::unique_ptr<IDBKey>>(
          script_state->GetIsolate(), primary_key_value, exception_state);
  if (exception_state.HadException())
    return;
  if (!primary_key->IsValid()) {
    exception_state.ThrowDOMException(kDataError,
                                      IDBDatabase::kNotValidKeyErrorMessage);
    return;
  }

  Continue(std::move(key), std::move(primary_key), std::move(metrics),
           exception_state);
}

void IDBCursor::Continue(std::unique_ptr<IDBKey> key,
                         std::unique_ptr<IDBKey> primary_key,
                         IDBRequest::AsyncTraceState metrics,
                         ExceptionState& exception_state) {
  DCHECK(transaction_->IsActive());
  DCHECK(got_value_);
  DCHECK(!IsDeleted());
  DCHECK(!primary_key || (key && primary_key));

  const IDBKey* current_primary_key = IdbPrimaryKey();

  if (key) {
    DCHECK(key_);
    if (direction_ == kWebIDBCursorDirectionNext ||
        direction_ == kWebIDBCursorDirectionNextNoDuplicate) {
      const bool ok = key_->IsLessThan(key.get()) ||
                      (primary_key && key_->IsEqual(key.get()) &&
                       current_primary_key->IsLessThan(primary_key.get()));
      if (!ok) {
        exception_state.ThrowDOMException(
            kDataError,
            "The parameter is less than or equal to this cursor's position.");
        return;
      }

    } else {
      const bool ok = key->IsLessThan(key_.get()) ||
                      (primary_key && key->IsEqual(key_.get()) &&
                       primary_key->IsLessThan(current_primary_key));
      if (!ok) {
        exception_state.ThrowDOMException(kDataError,
                                          "The parameter is greater than or "
                                          "equal to this cursor's position.");
        return;
      }
    }
  }

  // FIXME: We're not using the context from when continue was called, which
  // means the callback will be on the original context openCursor was called
  // on. Is this right?
  request_->SetPendingCursor(this);
  request_->AssignNewMetrics(std::move(metrics));
  got_value_ = false;
  backend_->Continue(WebIDBKeyView(key.get()), WebIDBKeyView(primary_key.get()),
                     request_->CreateWebCallbacks().release());
}

IDBRequest* IDBCursor::Delete(ScriptState* script_state,
                              ExceptionState& exception_state) {
  IDB_TRACE("IDBCursor::deleteRequestSetup");
  IDBRequest::AsyncTraceState metrics("IDBCursor::delete");
  if (!transaction_->IsActive()) {
    exception_state.ThrowDOMException(kTransactionInactiveError,
                                      transaction_->InactiveErrorMessage());
    return nullptr;
  }
  if (transaction_->IsReadOnly()) {
    exception_state.ThrowDOMException(
        kReadOnlyError,
        "The record may not be deleted inside a read-only transaction.");
    return nullptr;
  }
  if (IsDeleted()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kSourceDeletedErrorMessage);
    return nullptr;
  }
  if (!got_value_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kNoValueErrorMessage);
    return nullptr;
  }
  if (IsKeyCursor()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kIsKeyCursorErrorMessage);
    return nullptr;
  }
  if (!transaction_->BackendDB()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return nullptr;
  }

  IDBRequest* request = IDBRequest::Create(
      script_state, this, transaction_.Get(), std::move(metrics));
  transaction_->BackendDB()->Delete(
      transaction_->Id(), EffectiveObjectStore()->Id(),
      WebIDBKeyView(IdbPrimaryKey()), request->CreateWebCallbacks().release());
  return request;
}

void IDBCursor::PostSuccessHandlerCallback() {
  if (backend_)
    backend_->PostSuccessHandlerCallback();
}

void IDBCursor::Close() {
  value_ = nullptr;
  request_.Clear();
  backend_.reset();
}

ScriptValue IDBCursor::key(ScriptState* script_state) {
  key_dirty_ = false;
  return ScriptValue::From(script_state, key_.get());
}

ScriptValue IDBCursor::primaryKey(ScriptState* script_state) {
  primary_key_dirty_ = false;
  const IDBKey* primary_key = primary_key_unless_injected_.get();
  if (!primary_key) {
#if DCHECK_IS_ON()
    DCHECK(value_has_injected_primary_key_);

    IDBObjectStore* object_store = EffectiveObjectStore();
    DCHECK(object_store->autoIncrement() &&
           !object_store->IdbKeyPath().IsNull());
#endif  // DCHECK_IS_ON()

    primary_key = value_->Value()->PrimaryKey();
  }
  return ScriptValue::From(script_state, primary_key);
}

ScriptValue IDBCursor::value(ScriptState* script_state) {
  DCHECK(IsCursorWithValue());

  IDBAny* value;
  if (value_) {
    value = value_;
#if DCHECK_IS_ON()
    if (value_has_injected_primary_key_) {
      IDBObjectStore* object_store = EffectiveObjectStore();
      DCHECK(object_store->autoIncrement() &&
             !object_store->IdbKeyPath().IsNull());
      AssertPrimaryKeyValidOrInjectable(script_state, value_->Value());
    }
#endif  // DCHECK_IS_ON()

  } else {
    value = IDBAny::CreateUndefined();
  }

  value_dirty_ = false;
  ScriptValue script_value = ScriptValue::From(script_state, value);
  return script_value;
}

void IDBCursor::source(Source& source) const {
  source = source_;
}

void IDBCursor::SetValueReady(std::unique_ptr<IDBKey> key,
                              std::unique_ptr<IDBKey> primary_key,
                              std::unique_ptr<IDBValue> value) {
  key_ = std::move(key);
  key_dirty_ = true;

  primary_key_unless_injected_ = std::move(primary_key);
  primary_key_dirty_ = true;

  got_value_ = true;

  if (!IsCursorWithValue())
    return;

  value_dirty_ = true;
#if DCHECK_IS_ON()
  value_has_injected_primary_key_ = false;
#endif  // DCHECK_IS_ON()

  if (!value) {
    value_ = nullptr;
    return;
  }

  IDBObjectStore* object_store = EffectiveObjectStore();
  if (object_store->autoIncrement() && !object_store->IdbKeyPath().IsNull()) {
    value->SetInjectedPrimaryKey(std::move(primary_key_unless_injected_),
                                 object_store->IdbKeyPath());
#if DCHECK_IS_ON()
    value_has_injected_primary_key_ = true;
#endif  // DCHECK_IS_ON()
  }

  value_ = IDBAny::Create(std::move(value));
}

const IDBKey* IDBCursor::IdbPrimaryKey() const {
  if (primary_key_unless_injected_ || !value_)
    return primary_key_unless_injected_.get();

#if DCHECK_IS_ON()
  DCHECK(value_has_injected_primary_key_);
#endif  // DCHECK_IS_ON()
  return value_->Value()->PrimaryKey();
}

IDBObjectStore* IDBCursor::EffectiveObjectStore() const {
  if (source_.IsIDBObjectStore())
    return source_.GetAsIDBObjectStore();
  return source_.GetAsIDBIndex()->objectStore();
}

bool IDBCursor::IsDeleted() const {
  if (source_.IsIDBObjectStore())
    return source_.GetAsIDBObjectStore()->IsDeleted();
  return source_.GetAsIDBIndex()->IsDeleted();
}

WebIDBCursorDirection IDBCursor::StringToDirection(
    const String& direction_string) {
  if (direction_string == IndexedDBNames::next)
    return kWebIDBCursorDirectionNext;
  if (direction_string == IndexedDBNames::nextunique)
    return kWebIDBCursorDirectionNextNoDuplicate;
  if (direction_string == IndexedDBNames::prev)
    return kWebIDBCursorDirectionPrev;
  if (direction_string == IndexedDBNames::prevunique)
    return kWebIDBCursorDirectionPrevNoDuplicate;

  NOTREACHED();
  return kWebIDBCursorDirectionNext;
}

const String& IDBCursor::direction() const {
  switch (direction_) {
    case kWebIDBCursorDirectionNext:
      return IndexedDBNames::next;

    case kWebIDBCursorDirectionNextNoDuplicate:
      return IndexedDBNames::nextunique;

    case kWebIDBCursorDirectionPrev:
      return IndexedDBNames::prev;

    case kWebIDBCursorDirectionPrevNoDuplicate:
      return IndexedDBNames::prevunique;

    default:
      NOTREACHED();
      return IndexedDBNames::next;
  }
}

}  // namespace blink
