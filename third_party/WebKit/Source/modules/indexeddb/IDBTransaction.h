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

#ifndef IDBTransaction_h
#define IDBTransaction_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMStringList.h"
#include "core/events/EventListener.h"
#include "modules/EventModules.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IndexedDB.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "wtf/HashSet.h"

namespace blink {

class DOMException;
class ExceptionState;
class IDBDatabase;
class IDBObjectStore;
class IDBOpenDBRequest;

class MODULES_EXPORT IDBTransaction final : public EventTargetWithInlineData,
                                            public ActiveScriptWrappable,
                                            public ActiveDOMObject {
  USING_GARBAGE_COLLECTED_MIXIN(IDBTransaction);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBTransaction* createNonVersionChange(ScriptState*,
                                                int64_t,
                                                const HashSet<String>& scope,
                                                WebIDBTransactionMode,
                                                IDBDatabase*);
  static IDBTransaction* createVersionChange(
      ScriptState*,
      int64_t,
      IDBDatabase*,
      IDBOpenDBRequest*,
      const IDBDatabaseMetadata& oldMetadata);
  ~IDBTransaction() override;
  DECLARE_VIRTUAL_TRACE();

  static WebIDBTransactionMode stringToMode(const String&);

  // When the connection is closed backend will be 0.
  WebIDBDatabase* backendDB() const;

  int64_t id() const { return m_id; }
  bool isActive() const { return m_state == Active; }
  bool isFinished() const { return m_state == Finished; }
  bool isFinishing() const { return m_state == Finishing; }
  bool isReadOnly() const { return m_mode == WebIDBTransactionModeReadOnly; }
  bool isVersionChange() const {
    return m_mode == WebIDBTransactionModeVersionChange;
  }

  // Implement the IDBTransaction IDL
  const String& mode() const;
  DOMStringList* objectStoreNames() const;
  IDBDatabase* db() const { return m_database.get(); }
  DOMException* error() const { return m_error; }
  IDBObjectStore* objectStore(const String& name, ExceptionState&);
  void abort(ExceptionState&);

  void registerRequest(IDBRequest*);
  void unregisterRequest(IDBRequest*);
  void objectStoreCreated(const String&, IDBObjectStore*);
  void objectStoreDeleted(const String&);
  void objectStoreRenamed(const String& oldName, const String& newName);
  void setActive(bool);
  void setError(DOMException*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(complete);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

  void onAbort(DOMException*);
  void onComplete();

  // EventTarget
  const AtomicString& interfaceName() const override;
  ExecutionContext* getExecutionContext() const override;

  // ScriptWrappable
  bool hasPendingActivity() const final;

  // ActiveDOMObject
  void contextDestroyed() override;

 protected:
  // EventTarget
  DispatchEventResult dispatchEventInternal(Event*) override;

 private:
  IDBTransaction(ScriptState*,
                 int64_t,
                 const HashSet<String>&,
                 WebIDBTransactionMode,
                 IDBDatabase*,
                 IDBOpenDBRequest*,
                 const IDBDatabaseMetadata&);

  void enqueueEvent(Event*);

  // Called when a transaction is aborted.
  void abortOutstandingRequests();
  void revertDatabaseMetadata();

  // Called when a transaction is completed (committed or aborted).
  void finished();

  enum State {
    Inactive,   // Created or started, but not in an event callback
    Active,     // Created or started, in creation scope or an event callback
    Finishing,  // In the process of aborting or completing.
    Finished,   // No more events will fire and no new requests may be filed.
  };

  const int64_t m_id;
  Member<IDBDatabase> m_database;
  Member<IDBOpenDBRequest> m_openDBRequest;
  const WebIDBTransactionMode m_mode;

  // The names of the object stores that make up this transaction's scope.
  //
  // Transactions may not access object stores outside their scope.
  //
  // The scope of versionchange transactions is the entire database. We
  // represent this case with an empty m_scope, because copying all the store
  // names would waste both time and memory.
  //
  // Using object store names to represent a transaction's scope is safe
  // because object stores cannot be renamed in non-versionchange
  // transactions.
  const HashSet<String> m_scope;

  State m_state = Active;
  bool m_hasPendingActivity = true;
  bool m_contextStopped = false;
  Member<DOMException> m_error;

  HeapListHashSet<Member<IDBRequest>> m_requestList;

#if DCHECK_IS_ON()
  bool m_finishCalled = false;
#endif  // DCHECK_IS_ON()

  // Caches the IDBObjectStore instances returned by the objectStore() method.
  //
  // The spec requires that a transaction's objectStore() returns the same
  // IDBObjectStore instance for a specific store, so this cache is necessary
  // for correctness.
  //
  // objectStore() throws for completed/aborted transactions, so this is not
  // used after a transaction is finished, and can be cleared.
  using IDBObjectStoreMap = HeapHashMap<String, Member<IDBObjectStore>>;
  IDBObjectStoreMap m_objectStoreMap;

  // Used to mark stores created in an aborted upgrade transaction as
  // deleted.
  HeapHashSet<Member<IDBObjectStore>> m_createdObjectStores;

  // Used to notify object stores (which are no longer in m_objectStoreMap)
  // when the transaction is finished.
  HeapHashSet<Member<IDBObjectStore>> m_deletedObjectStores;

  // Holds stores created, deleted, or used during upgrade transactions to
  // reset metadata in case of abort.
  HeapHashMap<Member<IDBObjectStore>, IDBObjectStoreMetadata>
      m_objectStoreCleanupMap;

  IDBDatabaseMetadata m_oldDatabaseMetadata;
};

}  // namespace blink

#endif  // IDBTransaction_h
