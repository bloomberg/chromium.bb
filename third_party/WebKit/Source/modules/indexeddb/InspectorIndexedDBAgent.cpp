/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/indexeddb/InspectorIndexedDBAgent.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/events/EventListener.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/V8InspectorString.h"
#include "modules/indexed_db_names.h"
#include "modules/indexeddb/GlobalIndexedDB.h"
#include "modules/indexeddb/IDBCursor.h"
#include "modules/indexeddb/IDBCursorWithValue.h"
#include "modules/indexeddb/IDBDatabase.h"
#include "modules/indexeddb/IDBFactory.h"
#include "modules/indexeddb/IDBIndex.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBObjectStore.h"
#include "modules/indexeddb/IDBOpenDBRequest.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Vector.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

using blink::protocol::Array;
using blink::protocol::IndexedDB::DatabaseWithObjectStores;
using blink::protocol::IndexedDB::DataEntry;
using blink::protocol::IndexedDB::Key;
using blink::protocol::IndexedDB::KeyPath;
using blink::protocol::IndexedDB::KeyRange;
using blink::protocol::IndexedDB::ObjectStore;
using blink::protocol::IndexedDB::ObjectStoreIndex;
using blink::protocol::Maybe;
using blink::protocol::Response;

typedef blink::protocol::IndexedDB::Backend::RequestDatabaseNamesCallback
    RequestDatabaseNamesCallback;
typedef blink::protocol::IndexedDB::Backend::RequestDatabaseCallback
    RequestDatabaseCallback;
typedef blink::protocol::IndexedDB::Backend::RequestDataCallback
    RequestDataCallback;
typedef blink::protocol::IndexedDB::Backend::ClearObjectStoreCallback
    ClearObjectStoreCallback;
typedef blink::protocol::IndexedDB::Backend::DeleteDatabaseCallback
    DeleteDatabaseCallback;

namespace blink {

namespace IndexedDBAgentState {
static const char kIndexedDBAgentEnabled[] = "indexedDBAgentEnabled";
};

namespace {

static const char kIndexedDBObjectGroup[] = "indexeddb";
static const char kNoDocumentError[] = "No document for given frame found";

class GetDatabaseNamesCallback final : public EventListener {
  WTF_MAKE_NONCOPYABLE(GetDatabaseNamesCallback);

 public:
  static GetDatabaseNamesCallback* Create(
      std::unique_ptr<RequestDatabaseNamesCallback> request_callback,
      const String& security_origin) {
    return new GetDatabaseNamesCallback(std::move(request_callback),
                                        security_origin);
  }

  ~GetDatabaseNamesCallback() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    if (event->type() != EventTypeNames::success) {
      request_callback_->sendFailure(Response::Error("Unexpected event type."));
      return;
    }

    IDBRequest* idb_request = static_cast<IDBRequest*>(event->target());
    IDBAny* request_result = idb_request->ResultAsAny();
    if (request_result->GetType() != IDBAny::kDOMStringListType) {
      request_callback_->sendFailure(
          Response::Error("Unexpected result type."));
      return;
    }

    DOMStringList* database_names_list = request_result->DomStringList();
    std::unique_ptr<protocol::Array<String>> database_names =
        protocol::Array<String>::create();
    for (size_t i = 0; i < database_names_list->length(); ++i)
      database_names->addItem(database_names_list->item(i));
    request_callback_->sendSuccess(std::move(database_names));
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { EventListener::Trace(visitor); }

 private:
  GetDatabaseNamesCallback(
      std::unique_ptr<RequestDatabaseNamesCallback> request_callback,
      const String& security_origin)
      : EventListener(EventListener::kCPPEventListenerType),
        request_callback_(std::move(request_callback)),
        security_origin_(security_origin) {}
  std::unique_ptr<RequestDatabaseNamesCallback> request_callback_;
  String security_origin_;
};

class DeleteCallback final : public EventListener {
  WTF_MAKE_NONCOPYABLE(DeleteCallback);

 public:
  static DeleteCallback* Create(
      std::unique_ptr<DeleteDatabaseCallback> request_callback,
      const String& security_origin) {
    return new DeleteCallback(std::move(request_callback), security_origin);
  }

  ~DeleteCallback() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    if (event->type() != EventTypeNames::success) {
      request_callback_->sendFailure(
          Response::Error("Failed to delete database."));
      return;
    }
    request_callback_->sendSuccess();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { EventListener::Trace(visitor); }

 private:
  DeleteCallback(std::unique_ptr<DeleteDatabaseCallback> request_callback,
                 const String& security_origin)
      : EventListener(EventListener::kCPPEventListenerType),
        request_callback_(std::move(request_callback)),
        security_origin_(security_origin) {}
  std::unique_ptr<DeleteDatabaseCallback> request_callback_;
  String security_origin_;
};

template <typename RequestCallback>
class OpenDatabaseCallback;
template <typename RequestCallback>
class UpgradeDatabaseCallback;

template <typename RequestCallback>
class ExecutableWithDatabase
    : public RefCounted<ExecutableWithDatabase<RequestCallback>> {
 public:
  ExecutableWithDatabase(ScriptState* script_state)
      : script_state_(script_state) {}
  virtual ~ExecutableWithDatabase() {}
  void Start(IDBFactory* idb_factory,
             SecurityOrigin*,
             const String& database_name) {
    OpenDatabaseCallback<RequestCallback>* open_callback =
        OpenDatabaseCallback<RequestCallback>::Create(this);
    UpgradeDatabaseCallback<RequestCallback>* upgrade_callback =
        UpgradeDatabaseCallback<RequestCallback>::Create(this);
    DummyExceptionStateForTesting exception_state;
    IDBOpenDBRequest* idb_open_db_request =
        idb_factory->open(GetScriptState(), database_name, exception_state);
    if (exception_state.HadException()) {
      GetRequestCallback()->sendFailure(
          Response::Error("Could not open database."));
      return;
    }
    idb_open_db_request->addEventListener(EventTypeNames::upgradeneeded,
                                          upgrade_callback, false);
    idb_open_db_request->addEventListener(EventTypeNames::success,
                                          open_callback, false);
  }
  virtual void Execute(IDBDatabase*) = 0;
  virtual RequestCallback* GetRequestCallback() = 0;
  ExecutionContext* Context() const {
    return ExecutionContext::From(script_state_.get());
  }
  ScriptState* GetScriptState() const { return script_state_.get(); }

 private:
  RefPtr<ScriptState> script_state_;
};

template <typename RequestCallback>
class OpenDatabaseCallback final : public EventListener {
 public:
  static OpenDatabaseCallback* Create(
      ExecutableWithDatabase<RequestCallback>* executable_with_database) {
    return new OpenDatabaseCallback(executable_with_database);
  }

  ~OpenDatabaseCallback() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext* context, Event* event) override {
    if (event->type() != EventTypeNames::success) {
      executable_with_database_->GetRequestCallback()->sendFailure(
          Response::Error("Unexpected event type."));
      return;
    }

    IDBOpenDBRequest* idb_open_db_request =
        static_cast<IDBOpenDBRequest*>(event->target());
    IDBAny* request_result = idb_open_db_request->ResultAsAny();
    if (request_result->GetType() != IDBAny::kIDBDatabaseType) {
      executable_with_database_->GetRequestCallback()->sendFailure(
          Response::Error("Unexpected result type."));
      return;
    }

    IDBDatabase* idb_database = request_result->IdbDatabase();
    executable_with_database_->Execute(idb_database);
    V8PerIsolateData::From(
        executable_with_database_->GetScriptState()->GetIsolate())
        ->RunEndOfScopeTasks();
    idb_database->close();
  }

 private:
  OpenDatabaseCallback(
      ExecutableWithDatabase<RequestCallback>* executable_with_database)
      : EventListener(EventListener::kCPPEventListenerType),
        executable_with_database_(executable_with_database) {}
  RefPtr<ExecutableWithDatabase<RequestCallback>> executable_with_database_;
};

template <typename RequestCallback>
class UpgradeDatabaseCallback final : public EventListener {
 public:
  static UpgradeDatabaseCallback* Create(
      ExecutableWithDatabase<RequestCallback>* executable_with_database) {
    return new UpgradeDatabaseCallback(executable_with_database);
  }

  ~UpgradeDatabaseCallback() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext* context, Event* event) override {
    if (event->type() != EventTypeNames::upgradeneeded) {
      executable_with_database_->GetRequestCallback()->sendFailure(
          Response::Error("Unexpected event type."));
      return;
    }

    // If an "upgradeneeded" event comes through then the database that
    // had previously been enumerated was deleted. We don't want to
    // implicitly re-create it here, so abort the transaction.
    IDBOpenDBRequest* idb_open_db_request =
        static_cast<IDBOpenDBRequest*>(event->target());
    NonThrowableExceptionState exception_state;
    idb_open_db_request->transaction()->abort(exception_state);
    executable_with_database_->GetRequestCallback()->sendFailure(
        Response::Error("Aborted upgrade."));
  }

 private:
  UpgradeDatabaseCallback(
      ExecutableWithDatabase<RequestCallback>* executable_with_database)
      : EventListener(EventListener::kCPPEventListenerType),
        executable_with_database_(executable_with_database) {}
  RefPtr<ExecutableWithDatabase<RequestCallback>> executable_with_database_;
};

static IDBTransaction* TransactionForDatabase(
    ScriptState* script_state,
    IDBDatabase* idb_database,
    const String& object_store_name,
    const String& mode = IndexedDBNames::readonly) {
  DummyExceptionStateForTesting exception_state;
  StringOrStringSequence scope;
  scope.SetString(object_store_name);
  IDBTransaction* idb_transaction =
      idb_database->transaction(script_state, scope, mode, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return idb_transaction;
}

static IDBObjectStore* ObjectStoreForTransaction(
    IDBTransaction* idb_transaction,
    const String& object_store_name) {
  DummyExceptionStateForTesting exception_state;
  IDBObjectStore* idb_object_store =
      idb_transaction->objectStore(object_store_name, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return idb_object_store;
}

static IDBIndex* IndexForObjectStore(IDBObjectStore* idb_object_store,
                                     const String& index_name) {
  DummyExceptionStateForTesting exception_state;
  IDBIndex* idb_index = idb_object_store->index(index_name, exception_state);
  if (exception_state.HadException())
    return nullptr;
  return idb_index;
}

static std::unique_ptr<KeyPath> KeyPathFromIDBKeyPath(
    const IDBKeyPath& idb_key_path) {
  std::unique_ptr<KeyPath> key_path;
  switch (idb_key_path.GetType()) {
    case IDBKeyPath::kNullType:
      key_path = KeyPath::create().setType(KeyPath::TypeEnum::Null).build();
      break;
    case IDBKeyPath::kStringType:
      key_path = KeyPath::create()
                     .setType(KeyPath::TypeEnum::String)
                     .setString(idb_key_path.GetString())
                     .build();
      break;
    case IDBKeyPath::kArrayType: {
      key_path = KeyPath::create().setType(KeyPath::TypeEnum::Array).build();
      std::unique_ptr<protocol::Array<String>> array =
          protocol::Array<String>::create();
      const Vector<String>& string_array = idb_key_path.Array();
      for (size_t i = 0; i < string_array.size(); ++i)
        array->addItem(string_array[i]);
      key_path->setArray(std::move(array));
      break;
    }
    default:
      NOTREACHED();
  }

  return key_path;
}

class DatabaseLoader final
    : public ExecutableWithDatabase<RequestDatabaseCallback> {
 public:
  static RefPtr<DatabaseLoader> Create(
      ScriptState* script_state,
      std::unique_ptr<RequestDatabaseCallback> request_callback) {
    return WTF::AdoptRef(
        new DatabaseLoader(script_state, std::move(request_callback)));
  }

  ~DatabaseLoader() override {}

  void Execute(IDBDatabase* idb_database) override {
    const IDBDatabaseMetadata database_metadata = idb_database->Metadata();

    std::unique_ptr<protocol::Array<protocol::IndexedDB::ObjectStore>>
        object_stores =
            protocol::Array<protocol::IndexedDB::ObjectStore>::create();

    for (const auto& store_map_entry : database_metadata.object_stores) {
      const IDBObjectStoreMetadata& object_store_metadata =
          *store_map_entry.value;

      std::unique_ptr<protocol::Array<protocol::IndexedDB::ObjectStoreIndex>>
          indexes =
              protocol::Array<protocol::IndexedDB::ObjectStoreIndex>::create();

      for (const auto& metadata_map_entry : object_store_metadata.indexes) {
        const IDBIndexMetadata& index_metadata = *metadata_map_entry.value;

        std::unique_ptr<ObjectStoreIndex> object_store_index =
            ObjectStoreIndex::create()
                .setName(index_metadata.name)
                .setKeyPath(KeyPathFromIDBKeyPath(index_metadata.key_path))
                .setUnique(index_metadata.unique)
                .setMultiEntry(index_metadata.multi_entry)
                .build();
        indexes->addItem(std::move(object_store_index));
      }

      std::unique_ptr<ObjectStore> object_store =
          ObjectStore::create()
              .setName(object_store_metadata.name)
              .setKeyPath(KeyPathFromIDBKeyPath(object_store_metadata.key_path))
              .setAutoIncrement(object_store_metadata.auto_increment)
              .setIndexes(std::move(indexes))
              .build();
      object_stores->addItem(std::move(object_store));
    }
    std::unique_ptr<DatabaseWithObjectStores> result =
        DatabaseWithObjectStores::create()
            .setName(idb_database->name())
            .setVersion(idb_database->version())
            .setObjectStores(std::move(object_stores))
            .build();

    request_callback_->sendSuccess(std::move(result));
  }

  RequestDatabaseCallback* GetRequestCallback() override {
    return request_callback_.get();
  }

 private:
  DatabaseLoader(ScriptState* script_state,
                 std::unique_ptr<RequestDatabaseCallback> request_callback)
      : ExecutableWithDatabase(script_state),
        request_callback_(std::move(request_callback)) {}
  std::unique_ptr<RequestDatabaseCallback> request_callback_;
};

static IDBKey* IdbKeyFromInspectorObject(protocol::IndexedDB::Key* key) {
  IDBKey* idb_key;

  if (!key)
    return nullptr;
  String type = key->getType();

  DEFINE_STATIC_LOCAL(String, number, ("number"));
  DEFINE_STATIC_LOCAL(String, string, ("string"));
  DEFINE_STATIC_LOCAL(String, date, ("date"));
  DEFINE_STATIC_LOCAL(String, array, ("array"));

  if (type == number) {
    if (!key->hasNumber())
      return nullptr;
    idb_key = IDBKey::CreateNumber(key->getNumber(0));
  } else if (type == string) {
    if (!key->hasString())
      return nullptr;
    idb_key = IDBKey::CreateString(key->getString(String()));
  } else if (type == date) {
    if (!key->hasDate())
      return nullptr;
    idb_key = IDBKey::CreateDate(key->getDate(0));
  } else if (type == array) {
    IDBKey::KeyArray key_array;
    auto array = key->getArray(nullptr);
    for (size_t i = 0; array && i < array->length(); ++i)
      key_array.push_back(IdbKeyFromInspectorObject(array->get(i)));
    idb_key = IDBKey::CreateArray(key_array);
  } else {
    return nullptr;
  }

  return idb_key;
}

static IDBKeyRange* IdbKeyRangeFromKeyRange(
    protocol::IndexedDB::KeyRange* key_range) {
  IDBKey* idb_lower = IdbKeyFromInspectorObject(key_range->getLower(nullptr));
  if (key_range->hasLower() && !idb_lower)
    return nullptr;

  IDBKey* idb_upper = IdbKeyFromInspectorObject(key_range->getUpper(nullptr));
  if (key_range->hasUpper() && !idb_upper)
    return nullptr;

  IDBKeyRange::LowerBoundType lower_bound_type =
      key_range->getLowerOpen() ? IDBKeyRange::kLowerBoundOpen
                                : IDBKeyRange::kLowerBoundClosed;
  IDBKeyRange::UpperBoundType upper_bound_type =
      key_range->getUpperOpen() ? IDBKeyRange::kUpperBoundOpen
                                : IDBKeyRange::kUpperBoundClosed;
  return IDBKeyRange::Create(idb_lower, idb_upper, lower_bound_type,
                             upper_bound_type);
}

class DataLoader;

class OpenCursorCallback final : public EventListener {
 public:
  static OpenCursorCallback* Create(
      v8_inspector::V8InspectorSession* v8_session,
      ScriptState* script_state,
      std::unique_ptr<RequestDataCallback> request_callback,
      int skip_count,
      unsigned page_size) {
    return new OpenCursorCallback(v8_session, script_state,
                                  std::move(request_callback), skip_count,
                                  page_size);
  }

  ~OpenCursorCallback() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    if (event->type() != EventTypeNames::success) {
      request_callback_->sendFailure(Response::Error("Unexpected event type."));
      return;
    }

    IDBRequest* idb_request = static_cast<IDBRequest*>(event->target());
    IDBAny* request_result = idb_request->ResultAsAny();
    if (request_result->GetType() == IDBAny::kIDBValueType) {
      end(false);
      return;
    }
    if (request_result->GetType() != IDBAny::kIDBCursorWithValueType) {
      request_callback_->sendFailure(
          Response::Error("Unexpected result type."));
      return;
    }

    IDBCursorWithValue* idb_cursor = request_result->IdbCursorWithValue();

    if (skip_count_) {
      DummyExceptionStateForTesting exception_state;
      idb_cursor->advance(skip_count_, exception_state);
      if (exception_state.HadException()) {
        request_callback_->sendFailure(
            Response::Error("Could not advance cursor."));
      }
      skip_count_ = 0;
      return;
    }

    if (result_->length() == page_size_) {
      end(true);
      return;
    }

    // Continue cursor before making injected script calls, otherwise
    // transaction might be finished.
    DummyExceptionStateForTesting exception_state;
    idb_cursor->Continue(nullptr, nullptr, IDBRequest::AsyncTraceState(),
                         exception_state);
    if (exception_state.HadException()) {
      request_callback_->sendFailure(
          Response::Error("Could not continue cursor."));
      return;
    }

    Document* document =
        ToDocument(ExecutionContext::From(script_state_.get()));
    if (!document)
      return;
    ScriptState* script_state = script_state_.get();
    ScriptState::Scope scope(script_state);
    v8::Local<v8::Context> context = script_state->GetContext();
    v8_inspector::StringView object_group =
        ToV8InspectorStringView(kIndexedDBObjectGroup);
    std::unique_ptr<DataEntry> data_entry =
        DataEntry::create()
            .setKey(v8_session_->wrapObject(
                context, idb_cursor->key(script_state).V8Value(), object_group))
            .setPrimaryKey(v8_session_->wrapObject(
                context, idb_cursor->primaryKey(script_state).V8Value(),
                object_group))
            .setValue(v8_session_->wrapObject(
                context, idb_cursor->value(script_state).V8Value(),
                object_group))
            .build();
    result_->addItem(std::move(data_entry));
  }

  void end(bool has_more) {
    request_callback_->sendSuccess(std::move(result_), has_more);
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { EventListener::Trace(visitor); }

 private:
  OpenCursorCallback(v8_inspector::V8InspectorSession* v8_session,
                     ScriptState* script_state,
                     std::unique_ptr<RequestDataCallback> request_callback,
                     int skip_count,
                     unsigned page_size)
      : EventListener(EventListener::kCPPEventListenerType),
        v8_session_(v8_session),
        script_state_(script_state),
        request_callback_(std::move(request_callback)),
        skip_count_(skip_count),
        page_size_(page_size) {
    result_ = Array<DataEntry>::create();
  }

  v8_inspector::V8InspectorSession* v8_session_;
  RefPtr<ScriptState> script_state_;
  std::unique_ptr<RequestDataCallback> request_callback_;
  int skip_count_;
  unsigned page_size_;
  std::unique_ptr<Array<DataEntry>> result_;
};

class DataLoader final : public ExecutableWithDatabase<RequestDataCallback> {
 public:
  static RefPtr<DataLoader> Create(
      v8_inspector::V8InspectorSession* v8_session,
      ScriptState* script_state,
      std::unique_ptr<RequestDataCallback> request_callback,
      const String& object_store_name,
      const String& index_name,
      IDBKeyRange* idb_key_range,
      int skip_count,
      unsigned page_size) {
    return WTF::AdoptRef(new DataLoader(
        v8_session, script_state, std::move(request_callback),
        object_store_name, index_name, idb_key_range, skip_count, page_size));
  }

  ~DataLoader() override {}

  void Execute(IDBDatabase* idb_database) override {
    IDBTransaction* idb_transaction = TransactionForDatabase(
        GetScriptState(), idb_database, object_store_name_);
    if (!idb_transaction) {
      request_callback_->sendFailure(
          Response::Error("Could not get transaction"));
      return;
    }
    IDBObjectStore* idb_object_store =
        ObjectStoreForTransaction(idb_transaction, object_store_name_);
    if (!idb_object_store) {
      request_callback_->sendFailure(
          Response::Error("Could not get object store"));
      return;
    }

    IDBRequest* idb_request;
    if (!index_name_.IsEmpty()) {
      IDBIndex* idb_index = IndexForObjectStore(idb_object_store, index_name_);
      if (!idb_index) {
        request_callback_->sendFailure(Response::Error("Could not get index"));
        return;
      }

      idb_request = idb_index->openCursor(
          GetScriptState(), idb_key_range_.Get(), kWebIDBCursorDirectionNext);
    } else {
      idb_request = idb_object_store->openCursor(
          GetScriptState(), idb_key_range_.Get(), kWebIDBCursorDirectionNext);
    }
    OpenCursorCallback* open_cursor_callback = OpenCursorCallback::Create(
        v8_session_, GetScriptState(), std::move(request_callback_),
        skip_count_, page_size_);
    idb_request->addEventListener(EventTypeNames::success, open_cursor_callback,
                                  false);
  }

  RequestDataCallback* GetRequestCallback() override {
    return request_callback_.get();
  }
  DataLoader(v8_inspector::V8InspectorSession* v8_session,
             ScriptState* script_state,
             std::unique_ptr<RequestDataCallback> request_callback,
             const String& object_store_name,
             const String& index_name,
             IDBKeyRange* idb_key_range,
             int skip_count,
             unsigned page_size)
      : ExecutableWithDatabase(script_state),
        v8_session_(v8_session),
        request_callback_(std::move(request_callback)),
        object_store_name_(object_store_name),
        index_name_(index_name),
        idb_key_range_(idb_key_range),
        skip_count_(skip_count),
        page_size_(page_size) {}

  v8_inspector::V8InspectorSession* v8_session_;
  std::unique_ptr<RequestDataCallback> request_callback_;
  String object_store_name_;
  String index_name_;
  Persistent<IDBKeyRange> idb_key_range_;
  int skip_count_;
  unsigned page_size_;
};

}  // namespace

// static
InspectorIndexedDBAgent::InspectorIndexedDBAgent(
    InspectedFrames* inspected_frames,
    v8_inspector::V8InspectorSession* v8_session)
    : inspected_frames_(inspected_frames), v8_session_(v8_session) {}

InspectorIndexedDBAgent::~InspectorIndexedDBAgent() {}

void InspectorIndexedDBAgent::Restore() {
  if (state_->booleanProperty(IndexedDBAgentState::kIndexedDBAgentEnabled,
                              false)) {
    enable();
  }
}

void InspectorIndexedDBAgent::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  if (frame == inspected_frames_->Root()) {
    v8_session_->releaseObjectGroup(
        ToV8InspectorStringView(kIndexedDBObjectGroup));
  }
}

Response InspectorIndexedDBAgent::enable() {
  state_->setBoolean(IndexedDBAgentState::kIndexedDBAgentEnabled, true);
  return Response::OK();
}

Response InspectorIndexedDBAgent::disable() {
  state_->setBoolean(IndexedDBAgentState::kIndexedDBAgentEnabled, false);
  v8_session_->releaseObjectGroup(
      ToV8InspectorStringView(kIndexedDBObjectGroup));
  return Response::OK();
}

static Response AssertIDBFactory(Document* document, IDBFactory*& result) {
  LocalDOMWindow* dom_window = document->domWindow();
  if (!dom_window)
    return Response::Error("No IndexedDB factory for given frame found");
  IDBFactory* idb_factory = GlobalIndexedDB::indexedDB(*dom_window);

  if (!idb_factory)
    return Response::Error("No IndexedDB factory for given frame found");
  result = idb_factory;
  return Response::OK();
}

void InspectorIndexedDBAgent::requestDatabaseNames(
    const String& security_origin,
    std::unique_ptr<RequestDatabaseNamesCallback> request_callback) {
  LocalFrame* frame =
      inspected_frames_->FrameWithSecurityOrigin(security_origin);
  Document* document = frame ? frame->GetDocument() : nullptr;
  if (!document) {
    request_callback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idb_factory = nullptr;
  Response response = AssertIDBFactory(document, idb_factory);
  if (!response.isSuccess()) {
    request_callback->sendFailure(response);
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state) {
    request_callback->sendFailure(Response::InternalError());
    return;
  }
  ScriptState::Scope scope(script_state);
  DummyExceptionStateForTesting exception_state;
  IDBRequest* idb_request =
      idb_factory->GetDatabaseNames(script_state, exception_state);
  if (exception_state.HadException()) {
    request_callback->sendFailure(
        Response::Error("Could not obtain database names."));
    return;
  }
  idb_request->addEventListener(
      EventTypeNames::success,
      GetDatabaseNamesCallback::Create(
          std::move(request_callback),
          document->GetSecurityOrigin()->ToRawString()),
      false);
}

void InspectorIndexedDBAgent::requestDatabase(
    const String& security_origin,
    const String& database_name,
    std::unique_ptr<RequestDatabaseCallback> request_callback) {
  LocalFrame* frame =
      inspected_frames_->FrameWithSecurityOrigin(security_origin);
  Document* document = frame ? frame->GetDocument() : nullptr;
  if (!document) {
    request_callback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idb_factory = nullptr;
  Response response = AssertIDBFactory(document, idb_factory);
  if (!response.isSuccess()) {
    request_callback->sendFailure(response);
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state) {
    request_callback->sendFailure(Response::InternalError());
    return;
  }

  ScriptState::Scope scope(script_state);
  RefPtr<DatabaseLoader> database_loader =
      DatabaseLoader::Create(script_state, std::move(request_callback));
  database_loader->Start(idb_factory, document->GetSecurityOrigin(),
                         database_name);
}

void InspectorIndexedDBAgent::requestData(
    const String& security_origin,
    const String& database_name,
    const String& object_store_name,
    const String& index_name,
    int skip_count,
    int page_size,
    Maybe<protocol::IndexedDB::KeyRange> key_range,
    std::unique_ptr<RequestDataCallback> request_callback) {
  LocalFrame* frame =
      inspected_frames_->FrameWithSecurityOrigin(security_origin);
  Document* document = frame ? frame->GetDocument() : nullptr;
  if (!document) {
    request_callback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idb_factory = nullptr;
  Response response = AssertIDBFactory(document, idb_factory);
  if (!response.isSuccess()) {
    request_callback->sendFailure(response);
    return;
  }

  IDBKeyRange* idb_key_range =
      key_range.isJust() ? IdbKeyRangeFromKeyRange(key_range.fromJust())
                         : nullptr;
  if (key_range.isJust() && !idb_key_range) {
    request_callback->sendFailure(Response::Error("Can not parse key range."));
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state) {
    request_callback->sendFailure(Response::InternalError());
    return;
  }

  ScriptState::Scope scope(script_state);
  RefPtr<DataLoader> data_loader = DataLoader::Create(
      v8_session_, script_state, std::move(request_callback), object_store_name,
      index_name, idb_key_range, skip_count, page_size);
  data_loader->Start(idb_factory, document->GetSecurityOrigin(), database_name);
}

class ClearObjectStoreListener final : public EventListener {
  WTF_MAKE_NONCOPYABLE(ClearObjectStoreListener);

 public:
  static ClearObjectStoreListener* Create(
      std::unique_ptr<ClearObjectStoreCallback> request_callback) {
    return new ClearObjectStoreListener(std::move(request_callback));
  }

  ~ClearObjectStoreListener() override {}

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

  void handleEvent(ExecutionContext*, Event* event) override {
    if (event->type() != EventTypeNames::complete) {
      request_callback_->sendFailure(Response::Error("Unexpected event type."));
      return;
    }

    request_callback_->sendSuccess();
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { EventListener::Trace(visitor); }

 private:
  ClearObjectStoreListener(
      std::unique_ptr<ClearObjectStoreCallback> request_callback)
      : EventListener(EventListener::kCPPEventListenerType),
        request_callback_(std::move(request_callback)) {}

  std::unique_ptr<ClearObjectStoreCallback> request_callback_;
};

class ClearObjectStore final
    : public ExecutableWithDatabase<ClearObjectStoreCallback> {
 public:
  static RefPtr<ClearObjectStore> Create(
      ScriptState* script_state,
      const String& object_store_name,
      std::unique_ptr<ClearObjectStoreCallback> request_callback) {
    return WTF::AdoptRef(new ClearObjectStore(script_state, object_store_name,
                                              std::move(request_callback)));
  }

  ClearObjectStore(ScriptState* script_state,
                   const String& object_store_name,
                   std::unique_ptr<ClearObjectStoreCallback> request_callback)
      : ExecutableWithDatabase(script_state),
        object_store_name_(object_store_name),
        request_callback_(std::move(request_callback)) {}

  void Execute(IDBDatabase* idb_database) override {
    IDBTransaction* idb_transaction =
        TransactionForDatabase(GetScriptState(), idb_database,
                               object_store_name_, IndexedDBNames::readwrite);
    if (!idb_transaction) {
      request_callback_->sendFailure(
          Response::Error("Could not get transaction"));
      return;
    }
    IDBObjectStore* idb_object_store =
        ObjectStoreForTransaction(idb_transaction, object_store_name_);
    if (!idb_object_store) {
      request_callback_->sendFailure(
          Response::Error("Could not get object store"));
      return;
    }

    DummyExceptionStateForTesting exception_state;
    idb_object_store->clear(GetScriptState(), exception_state);
    DCHECK(!exception_state.HadException());
    if (exception_state.HadException()) {
      ExceptionCode ec = exception_state.Code();
      request_callback_->sendFailure(Response::Error(
          String::Format("Could not clear object store '%s': %d",
                         object_store_name_.Utf8().data(), ec)));
      return;
    }
    idb_transaction->addEventListener(
        EventTypeNames::complete,
        ClearObjectStoreListener::Create(std::move(request_callback_)), false);
  }

  ClearObjectStoreCallback* GetRequestCallback() override {
    return request_callback_.get();
  }

 private:
  const String object_store_name_;
  std::unique_ptr<ClearObjectStoreCallback> request_callback_;
};

void InspectorIndexedDBAgent::clearObjectStore(
    const String& security_origin,
    const String& database_name,
    const String& object_store_name,
    std::unique_ptr<ClearObjectStoreCallback> request_callback) {
  LocalFrame* frame =
      inspected_frames_->FrameWithSecurityOrigin(security_origin);
  Document* document = frame ? frame->GetDocument() : nullptr;
  if (!document) {
    request_callback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idb_factory = nullptr;
  Response response = AssertIDBFactory(document, idb_factory);
  if (!response.isSuccess()) {
    request_callback->sendFailure(response);
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state) {
    request_callback->sendFailure(Response::InternalError());
    return;
  }

  ScriptState::Scope scope(script_state);
  RefPtr<ClearObjectStore> clear_object_store = ClearObjectStore::Create(
      script_state, object_store_name, std::move(request_callback));
  clear_object_store->Start(idb_factory, document->GetSecurityOrigin(),
                            database_name);
}

void InspectorIndexedDBAgent::deleteDatabase(
    const String& security_origin,
    const String& database_name,
    std::unique_ptr<DeleteDatabaseCallback> request_callback) {
  LocalFrame* frame =
      inspected_frames_->FrameWithSecurityOrigin(security_origin);
  Document* document = frame ? frame->GetDocument() : nullptr;
  if (!document) {
    request_callback->sendFailure(Response::Error(kNoDocumentError));
    return;
  }
  IDBFactory* idb_factory = nullptr;
  Response response = AssertIDBFactory(document, idb_factory);
  if (!response.isSuccess()) {
    request_callback->sendFailure(response);
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state) {
    request_callback->sendFailure(Response::InternalError());
    return;
  }
  ScriptState::Scope scope(script_state);
  DummyExceptionStateForTesting exception_state;
  IDBRequest* idb_request = idb_factory->CloseConnectionsAndDeleteDatabase(
      script_state, database_name, exception_state);
  if (exception_state.HadException()) {
    request_callback->sendFailure(
        Response::Error("Could not delete database."));
    return;
  }
  idb_request->addEventListener(
      EventTypeNames::success,
      DeleteCallback::Create(std::move(request_callback),
                             document->GetSecurityOrigin()->ToRawString()),
      false);
}

DEFINE_TRACE(InspectorIndexedDBAgent) {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
