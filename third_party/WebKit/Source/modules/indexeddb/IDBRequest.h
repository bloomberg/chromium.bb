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

#ifndef IDBRequest_h
#define IDBRequest_h

#include <memory>

#include "base/macros.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/SuspendableObject.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "modules/indexeddb/IDBAny.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/IndexedDB.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptState.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {

class DOMException;
class ExceptionState;
class IDBCursor;
struct IDBDatabaseMetadata;
class IDBValue;

class MODULES_EXPORT IDBRequest : public EventTargetWithInlineData,
                                  public ActiveScriptWrappable<IDBRequest>,
                                  public SuspendableObject {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(IDBRequest);

 public:
  // Records async tracing, starting on contruction and ending on destruction or
  // a call to |RecordAndReset()|.
  class AsyncTraceState {
   public:
    AsyncTraceState() {}
    AsyncTraceState(const char* tracing_name, void*, size_t sub_id);
    ~AsyncTraceState();
    AsyncTraceState(AsyncTraceState&& other) {
      this->tracing_name_ = other.tracing_name_;
      this->id_ = other.id_;
      other.tracing_name_ = nullptr;
    }
    AsyncTraceState& operator=(AsyncTraceState&& rhs) {
      this->tracing_name_ = rhs.tracing_name_;
      this->id_ = rhs.id_;
      rhs.tracing_name_ = nullptr;
      return *this;
    }

    bool is_valid() const { return tracing_name_; }
    void RecordAndReset();

   private:
    const char* tracing_name_ = nullptr;
    const void* id_;

    DISALLOW_COPY_AND_ASSIGN(AsyncTraceState);
  };

  static IDBRequest* Create(ScriptState*,
                            IDBAny* source,
                            IDBTransaction*,
                            AsyncTraceState);
  ~IDBRequest() override;
  DECLARE_VIRTUAL_TRACE();

  v8::Isolate* GetIsolate() const { return isolate_; }
  ScriptValue result(ScriptState*, ExceptionState&);
  DOMException* error(ExceptionState&) const;
  ScriptValue source(ScriptState*) const;
  IDBTransaction* transaction() const { return transaction_.Get(); }

  bool isResultDirty() const { return result_dirty_; }
  IDBAny* ResultAsAny() const { return result_; }

  // Requests made during index population are implementation details and so
  // events should not be visible to script.
  void PreventPropagation() { prevent_propagation_ = true; }

  // Defined in the IDL
  enum ReadyState { PENDING = 1, DONE = 2, kEarlyDeath = 3 };

  const String& readyState() const;

  // Returns a new WebIDBCallbacks for this request.
  //
  // Each call must be paired with a WebCallbacksDestroyed() call. Most requests
  // have a single WebIDBCallbacks instance created for them.
  //
  // Requests used to open and iterate cursors are special, because they are
  // reused between openCursor() and continue() / advance() calls. These
  // requests have a new WebIDBCallbacks instance created for each of the
  // above-mentioned calls that they are involved in.
  std::unique_ptr<WebIDBCallbacks> CreateWebCallbacks();
  void WebCallbacksDestroyed() {
    DCHECK(web_callbacks_);
    web_callbacks_ = nullptr;
  }
#if DCHECK_IS_ON()
  WebIDBCallbacks* WebCallbacks() const { return web_callbacks_; }
#endif  // DCHECK_IS_ON()

  DEFINE_ATTRIBUTE_EVENT_LISTENER(success);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

  void SetCursorDetails(IndexedDB::CursorType, WebIDBCursorDirection);
  void SetPendingCursor(IDBCursor*);
  void Abort();

  // Blink's delivery of results from IndexedDB's backing store to script is
  // more complicated than prescribed in the IndexedDB specification.
  //
  // IDBValue, which holds responses from the backing store, is either the
  // serialized V8 value, or a reference to a Blob that holds the serialized
  // value. IDBValueWrapping.h has the motivation and details. This introduces
  // the following complexities.
  //
  // 1) De-serialization is expensive, so it is done lazily in
  // IDBRequest::result(), which is called synchronously from script. On the
  // other hand, Blob data can only be fetched asynchronously. So, IDBValues
  // that reference serialized data stored in Blobs must be processed before
  // IDBRequest event handlers are invoked, because the event handler script may
  // call IDBRequest::result().
  //
  // 2) The IDBRequest events must be dispatched (enqueued in DOMWindow's event
  // queue) in the order in which the requests were issued. If an IDBValue
  // references a Blob, the Blob processing must block event dispatch for all
  // following IDBRequests in the same transaction.
  //
  // The Blob de-referencing and IDBRequest blocking is performed in the
  // HandleResponse() overloads below. Each HandleResponse() overload is paired
  // with a matching EnqueueResponse() overload, which is called when an
  // IDBRequest's result event can be delivered to the application. All the
  // HandleResponse() variants include a fast path that calls directly into
  // EnqueueResponse() if no queueing is required.
  //
  // Some types of requests, such as indexedDB.openDatabase(), cannot be issued
  // after a request that needs Blob processing, so their results are handled by
  // having WebIDBCallbacksImpl call directly into EnqueueResponse(),
  // EnqueueBlocked(), or EnqueueUpgradeNeeded().

  void HandleResponse(DOMException*);
  void HandleResponse(IDBKey*);
  void HandleResponse(std::unique_ptr<WebIDBCursor>,
                      IDBKey*,
                      IDBKey* primary_key,
                      RefPtr<IDBValue>&&);
  void HandleResponse(IDBKey*, IDBKey* primary_key, RefPtr<IDBValue>&&);
  void HandleResponse(RefPtr<IDBValue>&&);
  void HandleResponse(const Vector<RefPtr<IDBValue>>&);
  void HandleResponse(int64_t);
  void HandleResponse();

  // Only used in webkitGetDatabaseNames(), which is deprecated and hopefully
  // going away soon.
  void EnqueueResponse(const Vector<String>&);

  // Only IDBOpenDBRequest instances should receive these:
  virtual void EnqueueBlocked(int64_t old_version) { NOTREACHED(); }
  virtual void EnqueueUpgradeNeeded(int64_t old_version,
                                    std::unique_ptr<WebIDBDatabase>,
                                    const IDBDatabaseMetadata&,
                                    WebIDBDataLoss,
                                    String data_loss_message) {
    NOTREACHED();
  }
  virtual void EnqueueResponse(std::unique_ptr<WebIDBDatabase>,
                               const IDBDatabaseMetadata&) {
    NOTREACHED();
  }

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // SuspendableObject
  void ContextDestroyed(ExecutionContext*) override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const final;
  void UncaughtExceptionInEventHandler() final;

  // Called by a version change transaction that has finished to set this
  // request back from DONE (following "upgradeneeded") back to PENDING (for
  // the upcoming "success" or "error").
  void TransactionDidFinishAndDispatch();

  IDBCursor* GetResultCursor() const;

  // Used to hang onto Blobs until the browser process handles the request.
  //
  // Blobs are ref-counted on the browser side, and BlobDataHandles manage
  // references from renderers. When a BlobDataHandle gets destroyed, the
  // browser-side Blob gets derefenced, which might cause it to be destroyed as
  // well.
  //
  // After script uses a Blob in a put() request, the Blink-side Blob object
  // (which hangs onto the BlobDataHandle) may get garbage-collected. IDBRequest
  // needs to hang onto the BlobDataHandle as well, to avoid having the
  // browser-side Blob get destroyed before the IndexedDB request is processed.
  inline Vector<RefPtr<BlobDataHandle>>* transit_blob_handles() {
    return &transit_blob_handles_;
  }

#if DCHECK_IS_ON()
  inline bool TransactionHasQueuedResults() const {
    return transaction_ && transaction_->HasQueuedResults();
  }
#endif  // DCHECK_IS_ON()

#if DCHECK_IS_ON()
  inline IDBRequestQueueItem* QueueItem() const { return queue_item_; }
#endif  // DCHECK_IS_ON()

  void AssignNewMetrics(AsyncTraceState metrics) {
    DCHECK(!metrics_.is_valid());
    metrics_ = std::move(metrics);
  }

 protected:
  IDBRequest(ScriptState*, IDBAny* source, IDBTransaction*, AsyncTraceState);
  void EnqueueEvent(Event*);
  void DequeueEvent(Event*);
  virtual bool ShouldEnqueueEvent() const;
  void EnqueueResultInternal(IDBAny*);
  void SetResult(IDBAny*);

  // Overridden by IDBOpenDBRequest.
  virtual void EnqueueResponse(int64_t);

  // EventTarget
  DispatchEventResult DispatchEventInternal(Event*) override;

  // Can be nullptr for requests that are not associated with a transaction,
  // i.e. delete requests and completed or unsuccessful open requests.
  Member<IDBTransaction> transaction_;

  ReadyState ready_state_ = PENDING;
  bool request_aborted_ = false;  // May be aborted by transaction then receive
                                  // async onsuccess; ignore vs. assert.
  // Maintain the isolate so that all externally allocated memory can be
  // registered against it.
  v8::Isolate* isolate_;

  AsyncTraceState metrics_;

 private:
  // Calls EnqueueResponse().
  friend class IDBRequestQueueItem;

  void SetResultCursor(IDBCursor*,
                       IDBKey*,
                       IDBKey* primary_key,
                       RefPtr<IDBValue>&&);
  void AckReceivedBlobs(const IDBValue*);
  void AckReceivedBlobs(const Vector<RefPtr<IDBValue>>&);

  void EnqueueResponse(DOMException*);
  void EnqueueResponse(IDBKey*);
  void EnqueueResponse(std::unique_ptr<WebIDBCursor>,
                       IDBKey*,
                       IDBKey* primary_key,
                       RefPtr<IDBValue>&&);
  void EnqueueResponse(IDBKey*, IDBKey* primary_key, RefPtr<IDBValue>&&);
  void EnqueueResponse(RefPtr<IDBValue>&&);
  void EnqueueResponse(const Vector<RefPtr<IDBValue>>&);
  void EnqueueResponse();

  void ClearPutOperationBlobs() { transit_blob_handles_.clear(); }

  Member<IDBAny> source_;
  Member<IDBAny> result_;
  Member<DOMException> error_;

  bool has_pending_activity_ = true;
  HeapVector<Member<Event>> enqueued_events_;

  // Only used if the result type will be a cursor.
  IndexedDB::CursorType cursor_type_ = IndexedDB::kCursorKeyAndValue;
  WebIDBCursorDirection cursor_direction_ = kWebIDBCursorDirectionNext;
  // When a cursor is continued/advanced, |result_| is cleared and
  // |pendingCursor_| holds it.
  Member<IDBCursor> pending_cursor_;
  // New state is not applied to the cursor object until the event is
  // dispatched.
  Member<IDBKey> cursor_key_;
  Member<IDBKey> cursor_primary_key_;
  RefPtr<IDBValue> cursor_value_;

  Vector<RefPtr<BlobDataHandle>> transit_blob_handles_;

  bool did_fire_upgrade_needed_event_ = false;
  bool prevent_propagation_ = false;
  bool result_dirty_ = true;

  // Transactions should be aborted after event dispatch if an exception was
  // not caught. This is cleared before dispatch, set by a call to
  // UncaughtExceptionInEventHandler() during dispatch, and checked afterwards
  // to abort if necessary.
  bool did_throw_in_event_handler_ = false;

  // Pointer back to the WebIDBCallbacks that holds a persistent reference to
  // this object.
  WebIDBCallbacks* web_callbacks_ = nullptr;

  // Non-null while this request is queued behind other requests that are still
  // getting post-processed.
  //
  // The IDBRequestQueueItem is owned by the result queue in IDBTransaction.
  IDBRequestQueueItem* queue_item_ = nullptr;
};

}  // namespace blink

#endif  // IDBRequest_h
