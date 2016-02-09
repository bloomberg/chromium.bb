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
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/events/EventListener.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectedFrames.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/DOMWindowIndexedDatabase.h"
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
#include "platform/JSONValues.h"
#include "platform/JSONValuesForV8.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "wtf/Vector.h"

using blink::TypeBuilder::Array;
using blink::TypeBuilder::IndexedDB::DatabaseWithObjectStores;
using blink::TypeBuilder::IndexedDB::DataEntry;
using blink::TypeBuilder::IndexedDB::Key;
using blink::TypeBuilder::IndexedDB::KeyPath;
using blink::TypeBuilder::IndexedDB::KeyRange;
using blink::TypeBuilder::IndexedDB::ObjectStore;
using blink::TypeBuilder::IndexedDB::ObjectStoreIndex;

typedef blink::InspectorBackendDispatcher::IndexedDBCommandHandler::RequestDatabaseNamesCallback RequestDatabaseNamesCallback;
typedef blink::InspectorBackendDispatcher::IndexedDBCommandHandler::RequestDatabaseCallback RequestDatabaseCallback;
typedef blink::InspectorBackendDispatcher::IndexedDBCommandHandler::RequestDataCallback RequestDataCallback;
typedef blink::InspectorBackendDispatcher::CallbackBase RequestCallback;
typedef blink::InspectorBackendDispatcher::IndexedDBCommandHandler::ClearObjectStoreCallback ClearObjectStoreCallback;

namespace blink {

namespace IndexedDBAgentState {
static const char indexedDBAgentEnabled[] = "indexedDBAgentEnabled";
};

namespace {

class GetDatabaseNamesCallback final : public EventListener {
    WTF_MAKE_NONCOPYABLE(GetDatabaseNamesCallback);
public:
    static PassRefPtrWillBeRawPtr<GetDatabaseNamesCallback> create(PassRefPtr<RequestDatabaseNamesCallback> requestCallback, const String& securityOrigin)
    {
        return adoptRefWillBeNoop(new GetDatabaseNamesCallback(requestCallback, securityOrigin));
    }

    ~GetDatabaseNamesCallback() override { }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ExecutionContext*, Event* event) override
    {
        if (!m_requestCallback->isActive())
            return;
        if (event->type() != EventTypeNames::success) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        IDBRequest* idbRequest = static_cast<IDBRequest*>(event->target());
        IDBAny* requestResult = idbRequest->resultAsAny();
        if (requestResult->type() != IDBAny::DOMStringListType) {
            m_requestCallback->sendFailure("Unexpected result type.");
            return;
        }

        RefPtrWillBeRawPtr<DOMStringList> databaseNamesList = requestResult->domStringList();
        RefPtr<TypeBuilder::Array<String>> databaseNames = TypeBuilder::Array<String>::create();
        for (size_t i = 0; i < databaseNamesList->length(); ++i)
            databaseNames->addItem(databaseNamesList->anonymousIndexedGetter(i));
        m_requestCallback->sendSuccess(databaseNames.release());
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        EventListener::trace(visitor);
    }

private:
    GetDatabaseNamesCallback(PassRefPtr<RequestDatabaseNamesCallback> requestCallback, const String& securityOrigin)
        : EventListener(EventListener::CPPEventListenerType)
        , m_requestCallback(requestCallback)
        , m_securityOrigin(securityOrigin) { }
    RefPtr<RequestDatabaseNamesCallback> m_requestCallback;
    String m_securityOrigin;
};

class ExecutableWithDatabase : public RefCounted<ExecutableWithDatabase> {
public:
    ExecutableWithDatabase(ScriptState* scriptState)
        : m_scriptState(scriptState) { }
    virtual ~ExecutableWithDatabase() { }
    void start(IDBFactory*, SecurityOrigin*, const String& databaseName);
    virtual void execute(IDBDatabase*) = 0;
    virtual RequestCallback* requestCallback() = 0;
    ExecutionContext* context() const { return m_scriptState->executionContext(); }
    ScriptState* scriptState() const { return m_scriptState.get(); }
private:
    RefPtr<ScriptState> m_scriptState;
};

class OpenDatabaseCallback final : public EventListener {
public:
    static PassRefPtrWillBeRawPtr<OpenDatabaseCallback> create(ExecutableWithDatabase* executableWithDatabase)
    {
        return adoptRefWillBeNoop(new OpenDatabaseCallback(executableWithDatabase));
    }

    ~OpenDatabaseCallback() override { }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ExecutionContext* context, Event* event) override
    {
        if (event->type() != EventTypeNames::success) {
            m_executableWithDatabase->requestCallback()->sendFailure("Unexpected event type.");
            return;
        }

        IDBOpenDBRequest* idbOpenDBRequest = static_cast<IDBOpenDBRequest*>(event->target());
        IDBAny* requestResult = idbOpenDBRequest->resultAsAny();
        if (requestResult->type() != IDBAny::IDBDatabaseType) {
            m_executableWithDatabase->requestCallback()->sendFailure("Unexpected result type.");
            return;
        }

        IDBDatabase* idbDatabase = requestResult->idbDatabase();
        m_executableWithDatabase->execute(idbDatabase);
        V8PerIsolateData::from(m_executableWithDatabase->scriptState()->isolate())->runEndOfScopeTasks();
        idbDatabase->close();
    }

private:
    OpenDatabaseCallback(ExecutableWithDatabase* executableWithDatabase)
        : EventListener(EventListener::CPPEventListenerType)
        , m_executableWithDatabase(executableWithDatabase) { }
    RefPtr<ExecutableWithDatabase> m_executableWithDatabase;
};

class UpgradeDatabaseCallback final : public EventListener {
public:
    static PassRefPtrWillBeRawPtr<UpgradeDatabaseCallback> create(ExecutableWithDatabase* executableWithDatabase)
    {
        return adoptRefWillBeNoop(new UpgradeDatabaseCallback(executableWithDatabase));
    }

    ~UpgradeDatabaseCallback() override { }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ExecutionContext* context, Event* event) override
    {
        if (event->type() != EventTypeNames::upgradeneeded) {
            m_executableWithDatabase->requestCallback()->sendFailure("Unexpected event type.");
            return;
        }

        // If an "upgradeneeded" event comes through then the database that
        // had previously been enumerated was deleted. We don't want to
        // implicitly re-create it here, so abort the transaction.
        IDBOpenDBRequest* idbOpenDBRequest = static_cast<IDBOpenDBRequest*>(event->target());
        NonThrowableExceptionState exceptionState;
        idbOpenDBRequest->transaction()->abort(exceptionState);
        m_executableWithDatabase->requestCallback()->sendFailure("Aborted upgrade.");
    }

private:
    UpgradeDatabaseCallback(ExecutableWithDatabase* executableWithDatabase)
        : EventListener(EventListener::CPPEventListenerType)
        , m_executableWithDatabase(executableWithDatabase) { }
    RefPtr<ExecutableWithDatabase> m_executableWithDatabase;
};

void ExecutableWithDatabase::start(IDBFactory* idbFactory, SecurityOrigin*, const String& databaseName)
{
    RefPtrWillBeRawPtr<OpenDatabaseCallback> openCallback = OpenDatabaseCallback::create(this);
    RefPtrWillBeRawPtr<UpgradeDatabaseCallback> upgradeCallback = UpgradeDatabaseCallback::create(this);
    TrackExceptionState exceptionState;
    IDBOpenDBRequest* idbOpenDBRequest = idbFactory->open(scriptState(), databaseName, exceptionState);
    if (exceptionState.hadException()) {
        requestCallback()->sendFailure("Could not open database.");
        return;
    }
    idbOpenDBRequest->addEventListener(EventTypeNames::upgradeneeded, upgradeCallback, false);
    idbOpenDBRequest->addEventListener(EventTypeNames::success, openCallback, false);
}

static IDBTransaction* transactionForDatabase(ScriptState* scriptState, IDBDatabase* idbDatabase, const String& objectStoreName, const String& mode = IndexedDBNames::readonly)
{
    TrackExceptionState exceptionState;
    StringOrStringSequenceOrDOMStringList scope;
    scope.setString(objectStoreName);
    IDBTransaction* idbTransaction = idbDatabase->transaction(scriptState, scope, mode, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return idbTransaction;
}

static IDBObjectStore* objectStoreForTransaction(IDBTransaction* idbTransaction, const String& objectStoreName)
{
    TrackExceptionState exceptionState;
    IDBObjectStore* idbObjectStore = idbTransaction->objectStore(objectStoreName, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return idbObjectStore;
}

static IDBIndex* indexForObjectStore(IDBObjectStore* idbObjectStore, const String& indexName)
{
    TrackExceptionState exceptionState;
    IDBIndex* idbIndex = idbObjectStore->index(indexName, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return idbIndex;
}

static PassRefPtr<KeyPath> keyPathFromIDBKeyPath(const IDBKeyPath& idbKeyPath)
{
    RefPtr<KeyPath> keyPath;
    switch (idbKeyPath.type()) {
    case IDBKeyPath::NullType:
        keyPath = KeyPath::create().setType(KeyPath::Type::Null);
        break;
    case IDBKeyPath::StringType:
        keyPath = KeyPath::create().setType(KeyPath::Type::String);
        keyPath->setString(idbKeyPath.string());
        break;
    case IDBKeyPath::ArrayType: {
        keyPath = KeyPath::create().setType(KeyPath::Type::Array);
        RefPtr<TypeBuilder::Array<String>> array = TypeBuilder::Array<String>::create();
        const Vector<String>& stringArray = idbKeyPath.array();
        for (size_t i = 0; i < stringArray.size(); ++i)
            array->addItem(stringArray[i]);
        keyPath->setArray(array);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }

    return keyPath.release();
}

class DatabaseLoader final : public ExecutableWithDatabase {
public:
    static PassRefPtr<DatabaseLoader> create(ScriptState* scriptState, PassRefPtr<RequestDatabaseCallback> requestCallback)
    {
        return adoptRef(new DatabaseLoader(scriptState, requestCallback));
    }

    ~DatabaseLoader() override { }

    void execute(IDBDatabase* idbDatabase) override
    {
        if (!requestCallback()->isActive())
            return;

        const IDBDatabaseMetadata databaseMetadata = idbDatabase->metadata();

        RefPtr<TypeBuilder::Array<TypeBuilder::IndexedDB::ObjectStore>> objectStores = TypeBuilder::Array<TypeBuilder::IndexedDB::ObjectStore>::create();

        for (const auto& storeMapEntry : databaseMetadata.objectStores) {
            const IDBObjectStoreMetadata& objectStoreMetadata = storeMapEntry.value;

            RefPtr<TypeBuilder::Array<TypeBuilder::IndexedDB::ObjectStoreIndex>> indexes = TypeBuilder::Array<TypeBuilder::IndexedDB::ObjectStoreIndex>::create();

            for (const auto& metadataMapEntry : objectStoreMetadata.indexes) {
                const IDBIndexMetadata& indexMetadata = metadataMapEntry.value;

                RefPtr<ObjectStoreIndex> objectStoreIndex = ObjectStoreIndex::create()
                    .setName(indexMetadata.name)
                    .setKeyPath(keyPathFromIDBKeyPath(indexMetadata.keyPath))
                    .setUnique(indexMetadata.unique)
                    .setMultiEntry(indexMetadata.multiEntry);
                indexes->addItem(objectStoreIndex);
            }

            RefPtr<ObjectStore> objectStore = ObjectStore::create()
                .setName(objectStoreMetadata.name)
                .setKeyPath(keyPathFromIDBKeyPath(objectStoreMetadata.keyPath))
                .setAutoIncrement(objectStoreMetadata.autoIncrement)
                .setIndexes(indexes);
            objectStores->addItem(objectStore);
        }
        RefPtr<DatabaseWithObjectStores> result = DatabaseWithObjectStores::create()
            .setName(databaseMetadata.name)
            .setIntVersion(databaseMetadata.intVersion)
            .setVersion(databaseMetadata.version)
            .setObjectStores(objectStores);

        m_requestCallback->sendSuccess(result);
    }

    RequestCallback* requestCallback() override { return m_requestCallback.get(); }
private:
    DatabaseLoader(ScriptState* scriptState, PassRefPtr<RequestDatabaseCallback> requestCallback)
        : ExecutableWithDatabase(scriptState)
        , m_requestCallback(requestCallback) { }
    RefPtr<RequestDatabaseCallback> m_requestCallback;
};

static IDBKey* idbKeyFromInspectorObject(JSONObject* key)
{
    IDBKey* idbKey;

    String type;
    if (!key->getString("type", &type))
        return nullptr;

    DEFINE_STATIC_LOCAL(String, s_number, ("number"));
    DEFINE_STATIC_LOCAL(String, s_string, ("string"));
    DEFINE_STATIC_LOCAL(String, s_date, ("date"));
    DEFINE_STATIC_LOCAL(String, s_array, ("array"));

    if (type == s_number) {
        double number;
        if (!key->getNumber("number", &number))
            return nullptr;
        idbKey = IDBKey::createNumber(number);
    } else if (type == s_string) {
        String string;
        if (!key->getString("string", &string))
            return nullptr;
        idbKey = IDBKey::createString(string);
    } else if (type == s_date) {
        double date;
        if (!key->getNumber("date", &date))
            return nullptr;
        idbKey = IDBKey::createDate(date);
    } else if (type == s_array) {
        IDBKey::KeyArray keyArray;
        RefPtr<JSONArray> array = key->getArray("array");
        for (size_t i = 0; i < array->length(); ++i) {
            RefPtr<JSONValue> value = array->get(i);
            RefPtr<JSONObject> object;
            if (!value->asObject(&object))
                return nullptr;
            keyArray.append(idbKeyFromInspectorObject(object.get()));
        }
        idbKey = IDBKey::createArray(keyArray);
    } else {
        return nullptr;
    }

    return idbKey;
}

static IDBKeyRange* idbKeyRangeFromKeyRange(JSONObject* keyRange)
{
    RefPtr<JSONObject> lower = keyRange->getObject("lower");
    IDBKey* idbLower = lower ? idbKeyFromInspectorObject(lower.get()) : nullptr;
    if (lower && !idbLower)
        return nullptr;

    RefPtr<JSONObject> upper = keyRange->getObject("upper");
    IDBKey* idbUpper = upper ? idbKeyFromInspectorObject(upper.get()) : nullptr;
    if (upper && !idbUpper)
        return nullptr;

    bool lowerOpen;
    if (!keyRange->getBoolean("lowerOpen", &lowerOpen))
        return nullptr;
    IDBKeyRange::LowerBoundType lowerBoundType = lowerOpen ? IDBKeyRange::LowerBoundOpen : IDBKeyRange::LowerBoundClosed;

    bool upperOpen;
    if (!keyRange->getBoolean("upperOpen", &upperOpen))
        return nullptr;
    IDBKeyRange::UpperBoundType upperBoundType = upperOpen ? IDBKeyRange::UpperBoundOpen : IDBKeyRange::UpperBoundClosed;

    return IDBKeyRange::create(idbLower, idbUpper, lowerBoundType, upperBoundType);
}

class DataLoader;

class OpenCursorCallback final : public EventListener {
public:
    static PassRefPtrWillBeRawPtr<OpenCursorCallback> create(ScriptState* scriptState, PassRefPtr<RequestDataCallback> requestCallback, int skipCount, unsigned pageSize)
    {
        return adoptRefWillBeNoop(new OpenCursorCallback(scriptState, requestCallback, skipCount, pageSize));
    }

    ~OpenCursorCallback() override { }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ExecutionContext*, Event* event) override
    {
        if (event->type() != EventTypeNames::success) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        IDBRequest* idbRequest = static_cast<IDBRequest*>(event->target());
        IDBAny* requestResult = idbRequest->resultAsAny();
        if (requestResult->type() == IDBAny::IDBValueType) {
            end(false);
            return;
        }
        if (requestResult->type() != IDBAny::IDBCursorWithValueType) {
            m_requestCallback->sendFailure("Unexpected result type.");
            return;
        }

        IDBCursorWithValue* idbCursor = requestResult->idbCursorWithValue();

        if (m_skipCount) {
            TrackExceptionState exceptionState;
            idbCursor->advance(m_skipCount, exceptionState);
            if (exceptionState.hadException())
                m_requestCallback->sendFailure("Could not advance cursor.");
            m_skipCount = 0;
            return;
        }

        if (m_result->length() == m_pageSize) {
            end(true);
            return;
        }

        // Continue cursor before making injected script calls, otherwise transaction might be finished.
        TrackExceptionState exceptionState;
        idbCursor->continueFunction(nullptr, nullptr, exceptionState);
        if (exceptionState.hadException()) {
            m_requestCallback->sendFailure("Could not continue cursor.");
            return;
        }

        Document* document = toDocument(m_scriptState->executionContext());
        if (!document)
            return;
        // FIXME: There are no tests for this error showing when a recursive
        // object is inspected.
        const String errorMessage("\"Inspection error. Maximum depth reached?\"");
        ScriptState* scriptState = m_scriptState.get();
        ScriptState::Scope scope(scriptState);
        RefPtr<JSONValue> keyJsonValue = toJSONValue(scriptState->context(), idbCursor->key(scriptState).v8Value());
        RefPtr<JSONValue> primaryKeyJsonValue = toJSONValue(scriptState->context(), idbCursor->primaryKey(scriptState).v8Value());
        RefPtr<JSONValue> valueJsonValue = toJSONValue(scriptState->context(), idbCursor->value(scriptState).v8Value());
        String key = keyJsonValue ? keyJsonValue->toJSONString() : errorMessage;
        String value = valueJsonValue ? valueJsonValue->toJSONString() : errorMessage;
        String primaryKey = primaryKeyJsonValue ? primaryKeyJsonValue->toJSONString() : errorMessage;
        RefPtr<DataEntry> dataEntry = DataEntry::create()
            .setKey(key)
            .setPrimaryKey(primaryKey)
            .setValue(value);
        m_result->addItem(dataEntry);
    }

    void end(bool hasMore)
    {
        if (!m_requestCallback->isActive())
            return;
        m_requestCallback->sendSuccess(m_result.release(), hasMore);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        EventListener::trace(visitor);
    }

private:
    OpenCursorCallback(ScriptState* scriptState, PassRefPtr<RequestDataCallback> requestCallback, int skipCount, unsigned pageSize)
        : EventListener(EventListener::CPPEventListenerType)
        , m_scriptState(scriptState)
        , m_requestCallback(requestCallback)
        , m_skipCount(skipCount)
        , m_pageSize(pageSize)
    {
        m_result = Array<DataEntry>::create();
    }

    RefPtr<ScriptState> m_scriptState;
    RefPtr<RequestDataCallback> m_requestCallback;
    int m_skipCount;
    unsigned m_pageSize;
    RefPtr<Array<DataEntry>> m_result;
};

class DataLoader final : public ExecutableWithDatabase {
public:
    static PassRefPtr<DataLoader> create(ScriptState* scriptState, PassRefPtr<RequestDataCallback> requestCallback, const String& objectStoreName, const String& indexName, IDBKeyRange* idbKeyRange, int skipCount, unsigned pageSize)
    {
        return adoptRef(new DataLoader(scriptState, requestCallback, objectStoreName, indexName, idbKeyRange, skipCount, pageSize));
    }

    ~DataLoader() override { }

    void execute(IDBDatabase* idbDatabase) override
    {
        if (!requestCallback()->isActive())
            return;
        IDBTransaction* idbTransaction = transactionForDatabase(scriptState(), idbDatabase, m_objectStoreName);
        if (!idbTransaction) {
            m_requestCallback->sendFailure("Could not get transaction");
            return;
        }
        IDBObjectStore* idbObjectStore = objectStoreForTransaction(idbTransaction, m_objectStoreName);
        if (!idbObjectStore) {
            m_requestCallback->sendFailure("Could not get object store");
            return;
        }

        RefPtrWillBeRawPtr<OpenCursorCallback> openCursorCallback = OpenCursorCallback::create(scriptState(), m_requestCallback, m_skipCount, m_pageSize);

        IDBRequest* idbRequest;
        if (!m_indexName.isEmpty()) {
            IDBIndex* idbIndex = indexForObjectStore(idbObjectStore, m_indexName);
            if (!idbIndex) {
                m_requestCallback->sendFailure("Could not get index");
                return;
            }

            idbRequest = idbIndex->openCursor(scriptState(), m_idbKeyRange.get(), WebIDBCursorDirectionNext);
        } else {
            idbRequest = idbObjectStore->openCursor(scriptState(), m_idbKeyRange.get(), WebIDBCursorDirectionNext);
        }
        idbRequest->addEventListener(EventTypeNames::success, openCursorCallback, false);
    }

    RequestCallback* requestCallback() override { return m_requestCallback.get(); }
    DataLoader(ScriptState* scriptState, PassRefPtr<RequestDataCallback> requestCallback, const String& objectStoreName, const String& indexName, IDBKeyRange* idbKeyRange, int skipCount, unsigned pageSize)
        : ExecutableWithDatabase(scriptState)
        , m_requestCallback(requestCallback)
        , m_objectStoreName(objectStoreName)
        , m_indexName(indexName)
        , m_idbKeyRange(idbKeyRange)
        , m_skipCount(skipCount)
        , m_pageSize(pageSize)
    {
    }

    RefPtr<RequestDataCallback> m_requestCallback;
    String m_objectStoreName;
    String m_indexName;
    Persistent<IDBKeyRange> m_idbKeyRange;
    int m_skipCount;
    unsigned m_pageSize;
};

} // namespace

// static
PassOwnPtrWillBeRawPtr<InspectorIndexedDBAgent> InspectorIndexedDBAgent::create(InspectedFrames* inspectedFrames)
{
    return adoptPtrWillBeNoop(new InspectorIndexedDBAgent(inspectedFrames));
}

InspectorIndexedDBAgent::InspectorIndexedDBAgent(InspectedFrames* inspectedFrames)
    : InspectorBaseAgent<InspectorIndexedDBAgent, InspectorFrontend::IndexedDB>("IndexedDB")
    , m_inspectedFrames(inspectedFrames)
{
}

InspectorIndexedDBAgent::~InspectorIndexedDBAgent()
{
}

void InspectorIndexedDBAgent::restore()
{
    if (m_state->booleanProperty(IndexedDBAgentState::indexedDBAgentEnabled, false)) {
        ErrorString error;
        enable(&error);
    }
}

void InspectorIndexedDBAgent::enable(ErrorString*)
{
    m_state->setBoolean(IndexedDBAgentState::indexedDBAgentEnabled, true);
}

void InspectorIndexedDBAgent::disable(ErrorString*)
{
    m_state->setBoolean(IndexedDBAgentState::indexedDBAgentEnabled, false);
}

static Document* assertDocument(ErrorString* errorString, LocalFrame* frame)
{
    Document* document = frame ? frame->document() : nullptr;

    if (!document)
        *errorString = "No document for given frame found";

    return document;
}

static IDBFactory* assertIDBFactory(ErrorString* errorString, Document* document)
{
    LocalDOMWindow* domWindow = document->domWindow();
    if (!domWindow) {
        *errorString = "No IndexedDB factory for given frame found";
        return nullptr;
    }
    IDBFactory* idbFactory = DOMWindowIndexedDatabase::indexedDB(*domWindow);

    if (!idbFactory)
        *errorString = "No IndexedDB factory for given frame found";

    return idbFactory;
}

void InspectorIndexedDBAgent::requestDatabaseNames(ErrorString* errorString, const String& securityOrigin, PassRefPtr<RequestDatabaseNamesCallback> requestCallback)
{
    LocalFrame* frame = m_inspectedFrames->frameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    ScriptState* scriptState = ScriptState::forMainWorld(frame);
    if (!scriptState)
        return;
    ScriptState::Scope scope(scriptState);
    TrackExceptionState exceptionState;
    IDBRequest* idbRequest = idbFactory->getDatabaseNames(scriptState, exceptionState);
    if (exceptionState.hadException()) {
        requestCallback->sendFailure("Could not obtain database names.");
        return;
    }
    idbRequest->addEventListener(EventTypeNames::success, GetDatabaseNamesCallback::create(requestCallback, document->securityOrigin()->toRawString()), false);
}

void InspectorIndexedDBAgent::requestDatabase(ErrorString* errorString, const String& securityOrigin, const String& databaseName, PassRefPtr<RequestDatabaseCallback> requestCallback)
{
    LocalFrame* frame = m_inspectedFrames->frameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    ScriptState* scriptState = ScriptState::forMainWorld(frame);
    if (!scriptState)
        return;
    ScriptState::Scope scope(scriptState);
    RefPtr<DatabaseLoader> databaseLoader = DatabaseLoader::create(scriptState, requestCallback);
    databaseLoader->start(idbFactory, document->securityOrigin(), databaseName);
}

void InspectorIndexedDBAgent::requestData(ErrorString* errorString, const String& securityOrigin, const String& databaseName, const String& objectStoreName, const String& indexName, int skipCount, int pageSize, const RefPtr<JSONObject>* keyRange, const PassRefPtr<RequestDataCallback> requestCallback)
{
    LocalFrame* frame = m_inspectedFrames->frameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    IDBKeyRange* idbKeyRange = keyRange ? idbKeyRangeFromKeyRange(keyRange->get()) : nullptr;
    if (keyRange && !idbKeyRange) {
        *errorString = "Can not parse key range.";
        return;
    }

    ScriptState* scriptState = ScriptState::forMainWorld(frame);
    if (!scriptState)
        return;
    ScriptState::Scope scope(scriptState);
    RefPtr<DataLoader> dataLoader = DataLoader::create(scriptState, requestCallback, objectStoreName, indexName, idbKeyRange, skipCount, pageSize);
    dataLoader->start(idbFactory, document->securityOrigin(), databaseName);
}

class ClearObjectStoreListener final : public EventListener {
    WTF_MAKE_NONCOPYABLE(ClearObjectStoreListener);
public:
    static PassRefPtrWillBeRawPtr<ClearObjectStoreListener> create(PassRefPtr<ClearObjectStoreCallback> requestCallback)
    {
        return adoptRefWillBeNoop(new ClearObjectStoreListener(requestCallback));
    }

    ~ClearObjectStoreListener() override { }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

    void handleEvent(ExecutionContext*, Event* event) override
    {
        if (!m_requestCallback->isActive())
            return;
        if (event->type() != EventTypeNames::complete) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        m_requestCallback->sendSuccess();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        EventListener::trace(visitor);
    }

private:
    ClearObjectStoreListener(PassRefPtr<ClearObjectStoreCallback> requestCallback)
        : EventListener(EventListener::CPPEventListenerType)
        , m_requestCallback(requestCallback)
    {
    }

    RefPtr<ClearObjectStoreCallback> m_requestCallback;
};


class ClearObjectStore final : public ExecutableWithDatabase {
public:
    static PassRefPtr<ClearObjectStore> create(ScriptState* scriptState, const String& objectStoreName, PassRefPtr<ClearObjectStoreCallback> requestCallback)
    {
        return adoptRef(new ClearObjectStore(scriptState, objectStoreName, requestCallback));
    }

    ClearObjectStore(ScriptState* scriptState, const String& objectStoreName, PassRefPtr<ClearObjectStoreCallback> requestCallback)
        : ExecutableWithDatabase(scriptState)
        , m_objectStoreName(objectStoreName)
        , m_requestCallback(requestCallback)
    {
    }

    void execute(IDBDatabase* idbDatabase) override
    {
        if (!requestCallback()->isActive())
            return;
        IDBTransaction* idbTransaction = transactionForDatabase(scriptState(), idbDatabase, m_objectStoreName, IndexedDBNames::readwrite);
        if (!idbTransaction) {
            m_requestCallback->sendFailure("Could not get transaction");
            return;
        }
        IDBObjectStore* idbObjectStore = objectStoreForTransaction(idbTransaction, m_objectStoreName);
        if (!idbObjectStore) {
            m_requestCallback->sendFailure("Could not get object store");
            return;
        }

        TrackExceptionState exceptionState;
        idbObjectStore->clear(scriptState(), exceptionState);
        ASSERT(!exceptionState.hadException());
        if (exceptionState.hadException()) {
            ExceptionCode ec = exceptionState.code();
            m_requestCallback->sendFailure(String::format("Could not clear object store '%s': %d", m_objectStoreName.utf8().data(), ec));
            return;
        }
        idbTransaction->addEventListener(EventTypeNames::complete, ClearObjectStoreListener::create(m_requestCallback), false);
    }

    RequestCallback* requestCallback() override { return m_requestCallback.get(); }
private:
    const String m_objectStoreName;
    RefPtr<ClearObjectStoreCallback> m_requestCallback;
};

void InspectorIndexedDBAgent::clearObjectStore(ErrorString* errorString, const String& securityOrigin, const String& databaseName, const String& objectStoreName, PassRefPtr<ClearObjectStoreCallback> requestCallback)
{
    LocalFrame* frame = m_inspectedFrames->frameWithSecurityOrigin(securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    ScriptState* scriptState = ScriptState::forMainWorld(frame);
    if (!scriptState)
        return;
    ScriptState::Scope scope(scriptState);
    RefPtr<ClearObjectStore> clearObjectStore = ClearObjectStore::create(scriptState, objectStoreName, requestCallback);
    clearObjectStore->start(idbFactory, document->securityOrigin(), databaseName);
}

DEFINE_TRACE(InspectorIndexedDBAgent)
{
    visitor->trace(m_inspectedFrames);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink
