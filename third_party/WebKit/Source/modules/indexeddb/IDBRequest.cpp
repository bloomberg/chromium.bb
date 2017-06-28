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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "modules/indexeddb/IDBRequest.h"

#include <memory>
#include <utility>

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventQueue.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/IDBCursorWithValue.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBEventDispatcher.h"
#include "modules/indexeddb/IDBRequestQueueItem.h"
#include "modules/indexeddb/IDBTracing.h"
#include "modules/indexeddb/IDBValue.h"
#include "modules/indexeddb/IDBValueWrapping.h"
#include "modules/indexeddb/WebIDBCallbacksImpl.h"
#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/WebBlobInfo.h"

using blink::WebIDBCursor;

namespace blink {

IDBRequest::AsyncTraceState::AsyncTraceState(const char* tracing_name,
                                             void* id,
                                             size_t sub_id)
    : tracing_name_(tracing_name), id_(static_cast<char*>(id) + sub_id) {
  if (tracing_name_)
    TRACE_EVENT_ASYNC_BEGIN0("IndexedDB", tracing_name_, id_);
}

void IDBRequest::AsyncTraceState::RecordAndReset() {
  if (tracing_name_)
    TRACE_EVENT_ASYNC_END0("IndexedDB", tracing_name_, id_);
  tracing_name_ = nullptr;
}

IDBRequest::AsyncTraceState::~AsyncTraceState() {
  if (tracing_name_)
    TRACE_EVENT_ASYNC_END0("IndexedDB", tracing_name_, id_);
}

IDBRequest* IDBRequest::Create(ScriptState* script_state,
                               IDBAny* source,
                               IDBTransaction* transaction,
                               IDBRequest::AsyncTraceState metrics) {
  IDBRequest* request =
      new IDBRequest(script_state, source, transaction, std::move(metrics));
  request->SuspendIfNeeded();
  // Requests associated with IDBFactory (open/deleteDatabase/getDatabaseNames)
  // do not have an associated transaction.
  if (transaction)
    transaction->RegisterRequest(request);
  return request;
}

IDBRequest::IDBRequest(ScriptState* script_state,
                       IDBAny* source,
                       IDBTransaction* transaction,
                       AsyncTraceState metrics)
    : SuspendableObject(ExecutionContext::From(script_state)),
      transaction_(transaction),
      isolate_(script_state->GetIsolate()),
      metrics_(std::move(metrics)),
      source_(source) {}

IDBRequest::~IDBRequest() {
  DCHECK((ready_state_ == DONE && !metrics_.is_valid()) ||
         ready_state_ == kEarlyDeath || !GetExecutionContext());
}

DEFINE_TRACE(IDBRequest) {
  visitor->Trace(transaction_);
  visitor->Trace(source_);
  visitor->Trace(result_);
  visitor->Trace(error_);
  visitor->Trace(enqueued_events_);
  visitor->Trace(pending_cursor_);
  visitor->Trace(cursor_key_);
  visitor->Trace(cursor_primary_key_);
  EventTargetWithInlineData::Trace(visitor);
  SuspendableObject::Trace(visitor);
}

ScriptValue IDBRequest::result(ScriptState* script_state,
                               ExceptionState& exception_state) {
  if (ready_state_ != DONE) {
    // Must throw if returning an empty value. Message is arbitrary since it
    // will never be seen.
    exception_state.ThrowDOMException(
        kInvalidStateError, IDBDatabase::kRequestNotFinishedErrorMessage);
    return ScriptValue();
  }
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      IDBDatabase::kDatabaseClosedErrorMessage);
    return ScriptValue();
  }
  result_dirty_ = false;
  ScriptValue value = ScriptValue::From(script_state, result_);
  return value;
}

DOMException* IDBRequest::error(ExceptionState& exception_state) const {
  if (ready_state_ != DONE) {
    exception_state.ThrowDOMException(
        kInvalidStateError, IDBDatabase::kRequestNotFinishedErrorMessage);
    return nullptr;
  }
  return error_;
}

ScriptValue IDBRequest::source(ScriptState* script_state) const {
  if (!GetExecutionContext())
    return ScriptValue();

  return ScriptValue::From(script_state, source_);
}

const String& IDBRequest::readyState() const {
  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);

  if (ready_state_ == PENDING)
    return IndexedDBNames::pending;

  return IndexedDBNames::done;
}

std::unique_ptr<WebIDBCallbacks> IDBRequest::CreateWebCallbacks() {
  DCHECK(!web_callbacks_);
  std::unique_ptr<WebIDBCallbacks> callbacks =
      WebIDBCallbacksImpl::Create(this);
  web_callbacks_ = callbacks.get();
  return callbacks;
}

void IDBRequest::Abort() {
  DCHECK(!request_aborted_);
  if (queue_item_) {
    queue_item_->CancelLoading();

    // A transaction's requests are aborted in order, so each aborted request
    // should immediately get out of the result queue.
    DCHECK(!queue_item_);
  }

  if (!GetExecutionContext())
    return;
  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);
  if (ready_state_ == DONE)
    return;

  EventQueue* event_queue = GetExecutionContext()->GetEventQueue();
  for (size_t i = 0; i < enqueued_events_.size(); ++i) {
    bool removed = event_queue->CancelEvent(enqueued_events_[i].Get());
    DCHECK(removed);
  }
  enqueued_events_.clear();

  error_.Clear();
  result_.Clear();
  EnqueueResponse(DOMException::Create(
      kAbortError,
      "The transaction was aborted, so the request cannot be fulfilled."));
  request_aborted_ = true;
}

void IDBRequest::SetCursorDetails(IndexedDB::CursorType cursor_type,
                                  WebIDBCursorDirection direction) {
  DCHECK_EQ(ready_state_, PENDING);
  DCHECK(!pending_cursor_);
  cursor_type_ = cursor_type;
  cursor_direction_ = direction;
}

void IDBRequest::SetPendingCursor(IDBCursor* cursor) {
  DCHECK_EQ(ready_state_, DONE);
  DCHECK(GetExecutionContext());
  DCHECK(transaction_);
  DCHECK(!pending_cursor_);
  DCHECK_EQ(cursor, GetResultCursor());

  has_pending_activity_ = true;
  pending_cursor_ = cursor;
  SetResult(nullptr);
  ready_state_ = PENDING;
  error_.Clear();
  transaction_->RegisterRequest(this);
}

IDBCursor* IDBRequest::GetResultCursor() const {
  if (!result_)
    return nullptr;
  if (result_->GetType() == IDBAny::kIDBCursorType)
    return result_->IdbCursor();
  if (result_->GetType() == IDBAny::kIDBCursorWithValueType)
    return result_->IdbCursorWithValue();
  return nullptr;
}

void IDBRequest::SetResultCursor(IDBCursor* cursor,
                                 IDBKey* key,
                                 IDBKey* primary_key,
                                 RefPtr<IDBValue>&& value) {
  DCHECK_EQ(ready_state_, PENDING);
  cursor_key_ = key;
  cursor_primary_key_ = primary_key;
  cursor_value_ = std::move(value);
  AckReceivedBlobs(cursor_value_.Get());

  EnqueueResultInternal(IDBAny::Create(cursor));
}

void IDBRequest::AckReceivedBlobs(const IDBValue* value) {
  if (!transaction_ || !transaction_->BackendDB())
    return;
  Vector<String> uuids = value->GetUUIDs();
  if (!uuids.IsEmpty())
    transaction_->BackendDB()->AckReceivedBlobs(uuids);
}

void IDBRequest::AckReceivedBlobs(const Vector<RefPtr<IDBValue>>& values) {
  for (size_t i = 0; i < values.size(); ++i)
    AckReceivedBlobs(values[i].Get());
}

bool IDBRequest::ShouldEnqueueEvent() const {
  const ExecutionContext* execution_context = GetExecutionContext();

  // https://crbug.com/733642 - Document::Shutdown() calls
  // LocalDOMWindow::ClearEventQueue(), which nulls out the context's event
  // queue, before calling ExecutionContext::NotifyContextDestroyed(). The
  // latter eventually calls IDBRequest::ContextDestroyed(), which aborts the
  // request. As an aborted IDBRequest is removed from its' IDBTransaction
  // result queue, it may unblock another request whose result is already
  // available. If the unblocked request hasn't received a
  // NotifyContextDestroyed() call yet, it will hang onto an ExecutionContext
  // whose event queue has been nulled out. The event queue null check covers
  // these specific circumstances.
  if (!execution_context || !execution_context->GetEventQueue())
    return false;

  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);
  if (request_aborted_)
    return false;
  DCHECK_EQ(ready_state_, PENDING);
  DCHECK(!error_ && !result_);
  return true;
}

void IDBRequest::HandleResponse(DOMException* error) {
  transit_blob_handles_.clear();
  if (!transaction_ || !transaction_->HasQueuedResults())
    return EnqueueResponse(error);
  transaction_->EnqueueResult(WTF::MakeUnique<IDBRequestQueueItem>(
      this, error,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(IDBKey* key) {
  transit_blob_handles_.clear();
  DCHECK(transaction_);
  if (!transaction_->HasQueuedResults())
    return EnqueueResponse(key);
  transaction_->EnqueueResult(WTF::MakeUnique<IDBRequestQueueItem>(
      this, key,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(int64_t value_or_old_version) {
  DCHECK(transit_blob_handles_.IsEmpty());
  if (!transaction_ || !transaction_->HasQueuedResults())
    return EnqueueResponse(value_or_old_version);
  transaction_->EnqueueResult(WTF::MakeUnique<IDBRequestQueueItem>(
      this, value_or_old_version,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse() {
  DCHECK(transit_blob_handles_.IsEmpty());
  if (!transaction_ || !transaction_->HasQueuedResults())
    return EnqueueResponse();
  transaction_->EnqueueResult(WTF::MakeUnique<IDBRequestQueueItem>(
      this, WTF::Bind(&IDBTransaction::OnResultReady,
                      WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(std::unique_ptr<WebIDBCursor> backend,
                                IDBKey* key,
                                IDBKey* primary_key,
                                RefPtr<IDBValue>&& value) {
  DCHECK(transit_blob_handles_.IsEmpty());
  DCHECK(transaction_);
  bool is_wrapped = IDBValueUnwrapper::IsWrapped(value.Get());
  if (!transaction_->HasQueuedResults() && !is_wrapped) {
    return EnqueueResponse(std::move(backend), key, primary_key,
                           std::move(value));
  }
  transaction_->EnqueueResult(WTF::MakeUnique<IDBRequestQueueItem>(
      this, std::move(backend), key, primary_key, std::move(value), is_wrapped,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(RefPtr<IDBValue>&& value) {
  DCHECK(transit_blob_handles_.IsEmpty());
  DCHECK(transaction_);
  bool is_wrapped = IDBValueUnwrapper::IsWrapped(value.Get());
  if (!transaction_->HasQueuedResults() && !is_wrapped)
    return EnqueueResponse(std::move(value));
  transaction_->EnqueueResult(WTF::MakeUnique<IDBRequestQueueItem>(
      this, std::move(value), is_wrapped,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(const Vector<RefPtr<IDBValue>>& values) {
  DCHECK(transit_blob_handles_.IsEmpty());
  DCHECK(transaction_);
  bool is_wrapped = IDBValueUnwrapper::IsWrapped(values);
  if (!transaction_->HasQueuedResults() && !is_wrapped)
    return EnqueueResponse(values);
  transaction_->EnqueueResult(WTF::MakeUnique<IDBRequestQueueItem>(
      this, values, is_wrapped,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::HandleResponse(IDBKey* key,
                                IDBKey* primary_key,
                                RefPtr<IDBValue>&& value) {
  DCHECK(transit_blob_handles_.IsEmpty());
  DCHECK(transaction_);
  bool is_wrapped = IDBValueUnwrapper::IsWrapped(value.Get());
  if (!transaction_->HasQueuedResults() && !is_wrapped)
    return EnqueueResponse(key, primary_key, std::move(value));

  transaction_->EnqueueResult(WTF::MakeUnique<IDBRequestQueueItem>(
      this, key, primary_key, std::move(value), is_wrapped,
      WTF::Bind(&IDBTransaction::OnResultReady,
                WrapPersistent(transaction_.Get()))));
}

void IDBRequest::EnqueueResponse(DOMException* error) {
  IDB_TRACE("IDBRequest::EnqueueResponse(DOMException)");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  error_ = error;
  SetResult(IDBAny::CreateUndefined());
  pending_cursor_.Clear();
  EnqueueEvent(Event::CreateCancelableBubble(EventTypeNames::error));
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResponse(const Vector<String>& string_list) {
  IDB_TRACE("IDBRequest::onSuccess(StringList)");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  DOMStringList* dom_string_list = DOMStringList::Create();
  for (size_t i = 0; i < string_list.size(); ++i)
    dom_string_list->Append(string_list[i]);
  EnqueueResultInternal(IDBAny::Create(dom_string_list));
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResponse(std::unique_ptr<WebIDBCursor> backend,
                                 IDBKey* key,
                                 IDBKey* primary_key,
                                 RefPtr<IDBValue>&& value) {
  IDB_TRACE1("IDBRequest::EnqueueResponse(IDBCursor)", "size",
             value ? value->DataSize() : 0);
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  DCHECK(!pending_cursor_);
  IDBCursor* cursor = nullptr;
  switch (cursor_type_) {
    case IndexedDB::kCursorKeyOnly:
      cursor = IDBCursor::Create(std::move(backend), cursor_direction_, this,
                                 source_.Get(), transaction_.Get());
      break;
    case IndexedDB::kCursorKeyAndValue:
      cursor =
          IDBCursorWithValue::Create(std::move(backend), cursor_direction_,
                                     this, source_.Get(), transaction_.Get());
      break;
    default:
      NOTREACHED();
  }
  SetResultCursor(cursor, key, primary_key, std::move(value));
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResponse(IDBKey* idb_key) {
  IDB_TRACE("IDBRequest::EnqueueResponse(IDBKey)");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  if (idb_key && idb_key->IsValid())
    EnqueueResultInternal(IDBAny::Create(idb_key));
  else
    EnqueueResultInternal(IDBAny::CreateUndefined());
  metrics_.RecordAndReset();
}

namespace {
size_t SizeOfValues(const Vector<RefPtr<IDBValue>>& values) {
  size_t size = 0;
  for (const auto& value : values)
    size += value->DataSize();
  return size;
}
}  // namespace

void IDBRequest::EnqueueResponse(const Vector<RefPtr<IDBValue>>& values) {
  IDB_TRACE1("IDBRequest::EnqueueResponse([IDBValue])", "size",
             SizeOfValues(values));
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  AckReceivedBlobs(values);
  EnqueueResultInternal(IDBAny::Create(values));
  metrics_.RecordAndReset();
}

#if DCHECK_IS_ON()
static IDBObjectStore* EffectiveObjectStore(IDBAny* source) {
  if (source->GetType() == IDBAny::kIDBObjectStoreType)
    return source->IdbObjectStore();
  if (source->GetType() == IDBAny::kIDBIndexType)
    return source->IdbIndex()->objectStore();

  NOTREACHED();
  return nullptr;
}
#endif  // DCHECK_IS_ON()

void IDBRequest::EnqueueResponse(RefPtr<IDBValue>&& value) {
  IDB_TRACE1("IDBRequest::EnqueueResponse(IDBValue)", "size",
             value ? value->DataSize() : 0);
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  AckReceivedBlobs(value.Get());

  if (pending_cursor_) {
    // Value should be null, signifying the end of the cursor's range.
    DCHECK(value->IsNull());
    DCHECK(!value->BlobInfo()->size());
    pending_cursor_->Close();
    pending_cursor_.Clear();
  }

#if DCHECK_IS_ON()
  DCHECK(!value->PrimaryKey() ||
         value->KeyPath() == EffectiveObjectStore(source_)->IdbKeyPath());
#endif

  EnqueueResultInternal(IDBAny::Create(std::move(value)));
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResponse(int64_t value) {
  IDB_TRACE("IDBRequest::EnqueueResponse(int64_t)");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }
  EnqueueResultInternal(IDBAny::Create(value));
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResponse() {
  IDB_TRACE("IDBRequest::EnqueueResponse()");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }
  EnqueueResultInternal(IDBAny::CreateUndefined());
  metrics_.RecordAndReset();
}

void IDBRequest::EnqueueResultInternal(IDBAny* result) {
  DCHECK(GetExecutionContext());
  DCHECK(!pending_cursor_);
  DCHECK(transit_blob_handles_.IsEmpty());
  SetResult(result);
  EnqueueEvent(Event::Create(EventTypeNames::success));
}

void IDBRequest::SetResult(IDBAny* result) {
  result_ = result;
  result_dirty_ = true;
}

void IDBRequest::EnqueueResponse(IDBKey* key,
                                 IDBKey* primary_key,
                                 RefPtr<IDBValue>&& value) {
  IDB_TRACE("IDBRequest::EnqueueResponse(IDBKey, IDBKey primaryKey, IDBValue)");
  if (!ShouldEnqueueEvent()) {
    metrics_.RecordAndReset();
    return;
  }

  DCHECK(pending_cursor_);
  SetResultCursor(pending_cursor_.Release(), key, primary_key,
                  std::move(value));
  metrics_.RecordAndReset();
}

bool IDBRequest::HasPendingActivity() const {
  // FIXME: In an ideal world, we should return true as long as anyone has a or
  //        can get a handle to us and we have event listeners. This is order to
  //        handle user generated events properly.
  return has_pending_activity_ && GetExecutionContext();
}

void IDBRequest::ContextDestroyed(ExecutionContext*) {
  if (ready_state_ == PENDING) {
    ready_state_ = kEarlyDeath;
    if (queue_item_)
      queue_item_->CancelLoading();
    if (transaction_)
      transaction_->UnregisterRequest(this);
  }

  enqueued_events_.clear();
  if (source_)
    source_->ContextWillBeDestroyed();
  if (result_)
    result_->ContextWillBeDestroyed();
  if (pending_cursor_)
    pending_cursor_->ContextWillBeDestroyed();
  if (web_callbacks_) {
    web_callbacks_->Detach();
    web_callbacks_ = nullptr;
  }
}

const AtomicString& IDBRequest::InterfaceName() const {
  return EventTargetNames::IDBRequest;
}

ExecutionContext* IDBRequest::GetExecutionContext() const {
  return SuspendableObject::GetExecutionContext();
}

DispatchEventResult IDBRequest::DispatchEventInternal(Event* event) {
  IDB_TRACE("IDBRequest::dispatchEvent");
  if (!GetExecutionContext())
    return DispatchEventResult::kCanceledBeforeDispatch;
  DCHECK_EQ(ready_state_, PENDING);
  DCHECK(has_pending_activity_);
  DCHECK(enqueued_events_.size());
  DCHECK_EQ(event->target(), this);

  if (event->type() != EventTypeNames::blocked)
    ready_state_ = DONE;
  DequeueEvent(event);

  HeapVector<Member<EventTarget>> targets;
  targets.push_back(this);
  if (transaction_ && !prevent_propagation_) {
    targets.push_back(transaction_);
    // If there ever are events that are associated with a database but
    // that do not have a transaction, then this will not work and we need
    // this object to actually hold a reference to the database (to ensure
    // it stays alive).
    targets.push_back(transaction_->db());
  }

  // Cursor properties should not be updated until the success event is being
  // dispatched.
  IDBCursor* cursor_to_notify = nullptr;
  if (event->type() == EventTypeNames::success) {
    cursor_to_notify = GetResultCursor();
    if (cursor_to_notify) {
      cursor_to_notify->SetValueReady(cursor_key_.Release(),
                                      cursor_primary_key_.Release(),
                                      std::move(cursor_value_));
    }
  }

  if (event->type() == EventTypeNames::upgradeneeded) {
    DCHECK(!did_fire_upgrade_needed_event_);
    did_fire_upgrade_needed_event_ = true;
  }

  // FIXME: When we allow custom event dispatching, this will probably need to
  // change.
  DCHECK(event->type() == EventTypeNames::success ||
         event->type() == EventTypeNames::error ||
         event->type() == EventTypeNames::blocked ||
         event->type() == EventTypeNames::upgradeneeded)
      << "event type was " << event->type();
  const bool set_transaction_active =
      transaction_ &&
      (event->type() == EventTypeNames::success ||
       event->type() == EventTypeNames::upgradeneeded ||
       (event->type() == EventTypeNames::error && !request_aborted_));

  if (set_transaction_active)
    transaction_->SetActive(true);

  // The request must be unregistered from the transaction before the event
  // handler is invoked, because the handler can call an IDBCursor method that
  // reuses this request, like continue() or advance(). http://crbug.com/724109
  // describes the consequences of getting this wrong.
  if (transaction_ && ready_state_ == DONE)
    transaction_->UnregisterRequest(this);

  did_throw_in_event_handler_ = false;
  DispatchEventResult dispatch_result =
      IDBEventDispatcher::Dispatch(event, targets);

  if (transaction_) {
    // Possibly abort the transaction. This must occur after unregistering (so
    // this request doesn't receive a second error) and before deactivating
    // (which might trigger commit).
    if (!request_aborted_) {
      if (did_throw_in_event_handler_) {
        transaction_->SetError(DOMException::Create(
            kAbortError, "Uncaught exception in event handler."));
        transaction_->abort(IGNORE_EXCEPTION_FOR_TESTING);
      } else if (event->type() == EventTypeNames::error &&
                 dispatch_result == DispatchEventResult::kNotCanceled) {
        transaction_->SetError(error_);
        transaction_->abort(IGNORE_EXCEPTION_FOR_TESTING);
      }
    }

    // If this was the last request in the transaction's list, it may commit
    // here.
    if (set_transaction_active)
      transaction_->SetActive(false);
  }

  if (cursor_to_notify)
    cursor_to_notify->PostSuccessHandlerCallback();

  // An upgradeneeded event will always be followed by a success or error event,
  // so must be kept alive.
  if (ready_state_ == DONE && event->type() != EventTypeNames::upgradeneeded)
    has_pending_activity_ = false;

  return dispatch_result;
}

void IDBRequest::UncaughtExceptionInEventHandler() {
  did_throw_in_event_handler_ = true;
}

void IDBRequest::TransactionDidFinishAndDispatch() {
  DCHECK(transaction_);
  DCHECK(transaction_->IsVersionChange());
  DCHECK(did_fire_upgrade_needed_event_);
  DCHECK_EQ(ready_state_, DONE);
  DCHECK(GetExecutionContext());
  transaction_.Clear();

  if (!GetExecutionContext())
    return;

  ready_state_ = PENDING;
}

void IDBRequest::EnqueueEvent(Event* event) {
  DCHECK(ready_state_ == PENDING || ready_state_ == DONE);

  if (!GetExecutionContext())
    return;

  DCHECK(ready_state_ == PENDING || did_fire_upgrade_needed_event_)
      << "When queueing event " << event->type() << ", ready_state_ was "
      << ready_state_;

  EventQueue* event_queue = GetExecutionContext()->GetEventQueue();
  event->SetTarget(this);

  // Keep track of enqueued events in case we need to abort prior to dispatch,
  // in which case these must be cancelled. If the events not dispatched for
  // other reasons they must be removed from this list via DequeueEvent().
  if (event_queue->EnqueueEvent(event))
    enqueued_events_.push_back(event);
}

void IDBRequest::DequeueEvent(Event* event) {
  for (size_t i = 0; i < enqueued_events_.size(); ++i) {
    if (enqueued_events_[i].Get() == event)
      enqueued_events_.erase(i);
  }
}

}  // namespace blink
