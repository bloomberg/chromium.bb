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

#ifndef IDBDatabase_h
#define IDBDatabase_h

#include <memory>

#include "bindings/modules/v8/string_or_string_sequence.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMStringList.h"
#include "modules/EventModules.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/indexeddb/IDBDatabaseCallbacks.h"
#include "modules/indexeddb/IDBHistograms.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBObjectStoreParameters.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "modules/indexeddb/IndexedDB.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"

namespace blink {

class DOMException;
class ExceptionState;
class ExecutionContext;
class IDBObserver;
struct WebIDBObservation;

class MODULES_EXPORT IDBDatabase final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<IDBDatabase>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(IDBDatabase);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBDatabase* Create(ExecutionContext*,
                             std::unique_ptr<WebIDBDatabase>,
                             IDBDatabaseCallbacks*,
                             v8::Isolate*);
  ~IDBDatabase() override;
  virtual void Trace(blink::Visitor*);
  virtual void TraceWrappers(const ScriptWrappableVisitor*) const;

  // Overwrites the database metadata, including object store and index
  // metadata. Used to pass metadata to the database when it is opened.
  void SetMetadata(const IDBDatabaseMetadata&);
  // Overwrites the database's own metadata, but does not change object store
  // and index metadata. Used to revert the database's metadata when a
  // versionchage transaction is aborted.
  void SetDatabaseMetadata(const IDBDatabaseMetadata&);
  void TransactionCreated(IDBTransaction*);
  void TransactionFinished(const IDBTransaction*);
  const String& GetObjectStoreName(int64_t object_store_id) const;
  int32_t AddObserver(
      IDBObserver*,
      int64_t transaction_id,
      bool include_transaction,
      bool no_records,
      bool values,
      const std::bitset<kWebIDBOperationTypeCount>& operation_types);
  void RemoveObservers(const Vector<int32_t>& observer_ids);

  // Implement the IDL
  const String& name() const { return metadata_.name; }
  unsigned long long version() const { return metadata_.version; }
  DOMStringList* objectStoreNames() const;

  IDBObjectStore* createObjectStore(const String& name,
                                    const IDBObjectStoreParameters& options,
                                    ExceptionState& exception_state) {
    return createObjectStore(name, IDBKeyPath(options.keyPath()),
                             options.autoIncrement(), exception_state);
  }
  IDBTransaction* transaction(ScriptState*,
                              const StringOrStringSequence& store_names,
                              const String& mode,
                              ExceptionState&);
  void deleteObjectStore(const String& name, ExceptionState&);
  void close();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(versionchange);

  // IDBDatabaseCallbacks
  void OnVersionChange(int64_t old_version, int64_t new_version);
  void OnAbort(int64_t, DOMException*);
  void OnComplete(int64_t);
  void OnChanges(const std::unordered_map<int32_t, std::vector<int32_t>>&
                     observation_index_map,
                 const WebVector<WebIDBObservation>& observations,
                 const IDBDatabaseCallbacks::TransactionMap& transactions);

  // ScriptWrappable
  bool HasPendingActivity() const final;

  // ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  bool IsClosePending() const { return close_pending_; }
  void ForceClose();
  const IDBDatabaseMetadata& Metadata() const { return metadata_; }
  void EnqueueEvent(Event*);

  int64_t FindObjectStoreId(const String& name) const;
  bool ContainsObjectStore(const String& name) const {
    return FindObjectStoreId(name) != IDBObjectStoreMetadata::kInvalidId;
  }
  void RenameObjectStore(int64_t store_id, const String& new_name);
  void RevertObjectStoreCreation(int64_t object_store_id);
  void RevertObjectStoreMetadata(
      scoped_refptr<IDBObjectStoreMetadata> old_metadata);

  // Will return nullptr if this database is stopped.
  WebIDBDatabase* Backend() const { return backend_.get(); }

  static int64_t NextTransactionId();
  static int32_t NextObserverId();

  static const char kCannotObserveVersionChangeTransaction[];
  static const char kIndexDeletedErrorMessage[];
  static const char kIndexNameTakenErrorMessage[];
  static const char kIsKeyCursorErrorMessage[];
  static const char kNoKeyOrKeyRangeErrorMessage[];
  static const char kNoSuchIndexErrorMessage[];
  static const char kNoSuchObjectStoreErrorMessage[];
  static const char kNoValueErrorMessage[];
  static const char kNotValidKeyErrorMessage[];
  static const char kNotVersionChangeTransactionErrorMessage[];
  static const char kObjectStoreDeletedErrorMessage[];
  static const char kObjectStoreNameTakenErrorMessage[];
  static const char kRequestNotFinishedErrorMessage[];
  static const char kSourceDeletedErrorMessage[];
  static const char kTransactionFinishedErrorMessage[];
  static const char kTransactionInactiveErrorMessage[];
  static const char kTransactionReadOnlyErrorMessage[];
  static const char kDatabaseClosedErrorMessage[];

  static void RecordApiCallsHistogram(IndexedDatabaseMethods);

 protected:
  // EventTarget
  DispatchEventResult DispatchEventInternal(Event*) override;

 private:
  IDBDatabase(ExecutionContext*,
              std::unique_ptr<WebIDBDatabase>,
              IDBDatabaseCallbacks*,
              v8::Isolate*);

  IDBObjectStore* createObjectStore(const String& name,
                                    const IDBKeyPath&,
                                    bool auto_increment,
                                    ExceptionState&);
  void CloseConnection();

  IDBDatabaseMetadata metadata_;
  std::unique_ptr<WebIDBDatabase> backend_;
  Member<IDBTransaction> version_change_transaction_;
  HeapHashMap<int64_t, Member<IDBTransaction>> transactions_;
  HeapHashMap<int32_t, TraceWrapperMember<IDBObserver>> observers_;

  bool close_pending_ = false;

  // Keep track of the versionchange events waiting to be fired on this
  // database so that we can cancel them if the database closes.
  HeapVector<Member<Event>> enqueued_events_;

  Member<IDBDatabaseCallbacks> database_callbacks_;
  // Maintain the isolate so that all externally allocated memory can be
  // registered against it.
  v8::Isolate* isolate_;
};

}  // namespace blink

#endif  // IDBDatabase_h
