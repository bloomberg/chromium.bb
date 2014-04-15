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

#include "config.h"
#include "modules/indexeddb/InspectorIndexedDBAgent.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptState.h"
#include "core/dom/DOMStringList.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorState.h"
#include "core/page/Page.h"
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
#include "modules/indexeddb/IDBPendingTransactionMonitor.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBTransaction.h"
#include "platform/JSONValues.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebIDBCursor.h"
#include "wtf/Vector.h"

using WebCore::TypeBuilder::Array;
using WebCore::TypeBuilder::IndexedDB::DatabaseWithObjectStores;
using WebCore::TypeBuilder::IndexedDB::DataEntry;
using WebCore::TypeBuilder::IndexedDB::Key;
using WebCore::TypeBuilder::IndexedDB::KeyPath;
using WebCore::TypeBuilder::IndexedDB::KeyRange;
using WebCore::TypeBuilder::IndexedDB::ObjectStore;
using WebCore::TypeBuilder::IndexedDB::ObjectStoreIndex;

typedef WebCore::InspectorBackendDispatcher::IndexedDBCommandHandler::RequestDatabaseNamesCallback RequestDatabaseNamesCallback;
typedef WebCore::InspectorBackendDispatcher::IndexedDBCommandHandler::RequestDatabaseCallback RequestDatabaseCallback;
typedef WebCore::InspectorBackendDispatcher::IndexedDBCommandHandler::RequestDataCallback RequestDataCallback;
typedef WebCore::InspectorBackendDispatcher::CallbackBase RequestCallback;
typedef WebCore::InspectorBackendDispatcher::IndexedDBCommandHandler::ClearObjectStoreCallback ClearObjectStoreCallback;

namespace WebCore {

namespace IndexedDBAgentState {
static const char indexedDBAgentEnabled[] = "indexedDBAgentEnabled";
};

namespace {

class GetDatabaseNamesCallback FINAL : public EventListener {
    WTF_MAKE_NONCOPYABLE(GetDatabaseNamesCallback);
public:
    static PassRefPtr<GetDatabaseNamesCallback> create(PassRefPtr<RequestDatabaseNamesCallback> requestCallback, const String& securityOrigin)
    {
        return adoptRef(new GetDatabaseNamesCallback(requestCallback, securityOrigin));
    }

    virtual ~GetDatabaseNamesCallback() { }

    virtual bool operator==(const EventListener& other) OVERRIDE
    {
        return this == &other;
    }

    virtual void handleEvent(ExecutionContext*, Event* event) OVERRIDE
    {
        if (!m_requestCallback->isActive())
            return;
        if (event->type() != EventTypeNames::success) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        IDBRequest* idbRequest = static_cast<IDBRequest*>(event->target());
        RefPtr<IDBAny> requestResult = idbRequest->resultAsAny();
        if (requestResult->type() != IDBAny::DOMStringListType) {
            m_requestCallback->sendFailure("Unexpected result type.");
            return;
        }

        RefPtr<DOMStringList> databaseNamesList = requestResult->domStringList();
        RefPtr<TypeBuilder::Array<String> > databaseNames = TypeBuilder::Array<String>::create();
        for (size_t i = 0; i < databaseNamesList->length(); ++i)
            databaseNames->addItem(databaseNamesList->item(i));
        m_requestCallback->sendSuccess(databaseNames.release());
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
    ExecutableWithDatabase(NewScriptState* scriptState)
        : m_scriptState(scriptState) { }
    virtual ~ExecutableWithDatabase() { };
    void start(IDBFactory*, SecurityOrigin*, const String& databaseName);
    virtual void execute(PassRefPtr<IDBDatabase>) = 0;
    virtual RequestCallback* requestCallback() = 0;
    ExecutionContext* context() const { return m_scriptState->executionContext(); }
    NewScriptState* scriptState() const { return m_scriptState.get(); }
private:
    RefPtr<NewScriptState> m_scriptState;
};

class OpenDatabaseCallback FINAL : public EventListener {
public:
    static PassRefPtr<OpenDatabaseCallback> create(ExecutableWithDatabase* executableWithDatabase)
    {
        return adoptRef(new OpenDatabaseCallback(executableWithDatabase));
    }

    virtual ~OpenDatabaseCallback() { }

    virtual bool operator==(const EventListener& other) OVERRIDE
    {
        return this == &other;
    }

    virtual void handleEvent(ExecutionContext*, Event* event) OVERRIDE
    {
        if (event->type() != EventTypeNames::success) {
            m_executableWithDatabase->requestCallback()->sendFailure("Unexpected event type.");
            return;
        }

        IDBOpenDBRequest* idbOpenDBRequest = static_cast<IDBOpenDBRequest*>(event->target());
        RefPtr<IDBAny> requestResult = idbOpenDBRequest->resultAsAny();
        if (requestResult->type() != IDBAny::IDBDatabaseType) {
            m_executableWithDatabase->requestCallback()->sendFailure("Unexpected result type.");
            return;
        }

        RefPtr<IDBDatabase> idbDatabase = requestResult->idbDatabase();
        m_executableWithDatabase->execute(idbDatabase);
        IDBPendingTransactionMonitor::deactivateNewTransactions();
        idbDatabase->close();
    }

private:
    OpenDatabaseCallback(ExecutableWithDatabase* executableWithDatabase)
        : EventListener(EventListener::CPPEventListenerType)
        , m_executableWithDatabase(executableWithDatabase) { }
    RefPtr<ExecutableWithDatabase> m_executableWithDatabase;
};

void ExecutableWithDatabase::start(IDBFactory* idbFactory, SecurityOrigin*, const String& databaseName)
{
    RefPtr<OpenDatabaseCallback> callback = OpenDatabaseCallback::create(this);
    TrackExceptionState exceptionState;
    RefPtr<IDBOpenDBRequest> idbOpenDBRequest = idbFactory->open(context(), databaseName, exceptionState);
    if (exceptionState.hadException()) {
        requestCallback()->sendFailure("Could not open database.");
        return;
    }
    idbOpenDBRequest->addEventListener(EventTypeNames::success, callback, false);
}

static PassRefPtr<IDBTransaction> transactionForDatabase(ExecutionContext* executionContext, IDBDatabase* idbDatabase, const String& objectStoreName, const String& mode = IDBTransaction::modeReadOnly())
{
    TrackExceptionState exceptionState;
    RefPtr<IDBTransaction> idbTransaction = idbDatabase->transaction(executionContext, objectStoreName, mode, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return idbTransaction;
}

static PassRefPtr<IDBObjectStore> objectStoreForTransaction(IDBTransaction* idbTransaction, const String& objectStoreName)
{
    TrackExceptionState exceptionState;
    RefPtr<IDBObjectStore> idbObjectStore = idbTransaction->objectStore(objectStoreName, exceptionState);
    if (exceptionState.hadException())
        return nullptr;
    return idbObjectStore;
}

static PassRefPtr<IDBIndex> indexForObjectStore(IDBObjectStore* idbObjectStore, const String& indexName)
{
    TrackExceptionState exceptionState;
    RefPtr<IDBIndex> idbIndex = idbObjectStore->index(indexName, exceptionState);
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
        RefPtr<TypeBuilder::Array<String> > array = TypeBuilder::Array<String>::create();
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

class DatabaseLoader FINAL : public ExecutableWithDatabase {
public:
    static PassRefPtr<DatabaseLoader> create(NewScriptState* scriptState, PassRefPtr<RequestDatabaseCallback> requestCallback)
    {
        return adoptRef(new DatabaseLoader(scriptState, requestCallback));
    }

    virtual ~DatabaseLoader() { }

    virtual void execute(PassRefPtr<IDBDatabase> prpDatabase) OVERRIDE
    {
        RefPtr<IDBDatabase> idbDatabase = prpDatabase;
        if (!requestCallback()->isActive())
            return;

        const IDBDatabaseMetadata databaseMetadata = idbDatabase->metadata();

        RefPtr<TypeBuilder::Array<TypeBuilder::IndexedDB::ObjectStore> > objectStores = TypeBuilder::Array<TypeBuilder::IndexedDB::ObjectStore>::create();

        for (IDBDatabaseMetadata::ObjectStoreMap::const_iterator it = databaseMetadata.objectStores.begin(); it != databaseMetadata.objectStores.end(); ++it) {
            const IDBObjectStoreMetadata& objectStoreMetadata = it->value;

            RefPtr<TypeBuilder::Array<TypeBuilder::IndexedDB::ObjectStoreIndex> > indexes = TypeBuilder::Array<TypeBuilder::IndexedDB::ObjectStoreIndex>::create();

            for (IDBObjectStoreMetadata::IndexMap::const_iterator it = objectStoreMetadata.indexes.begin(); it != objectStoreMetadata.indexes.end(); ++it) {
                const IDBIndexMetadata& indexMetadata = it->value;

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

    virtual RequestCallback* requestCallback() OVERRIDE { return m_requestCallback.get(); }
private:
    DatabaseLoader(NewScriptState* scriptState, PassRefPtr<RequestDatabaseCallback> requestCallback)
        : ExecutableWithDatabase(scriptState)
        , m_requestCallback(requestCallback) { }
    RefPtr<RequestDatabaseCallback> m_requestCallback;
};

static PassRefPtr<IDBKey> idbKeyFromInspectorObject(JSONObject* key)
{
    RefPtr<IDBKey> idbKey;

    String type;
    if (!key->getString("type", &type))
        return nullptr;

    DEFINE_STATIC_LOCAL(String, number, ("number"));
    DEFINE_STATIC_LOCAL(String, string, ("string"));
    DEFINE_STATIC_LOCAL(String, date, ("date"));
    DEFINE_STATIC_LOCAL(String, array, ("array"));

    if (type == number) {
        double number;
        if (!key->getNumber("number", &number))
            return nullptr;
        idbKey = IDBKey::createNumber(number);
    } else if (type == string) {
        String string;
        if (!key->getString("string", &string))
            return nullptr;
        idbKey = IDBKey::createString(string);
    } else if (type == date) {
        double date;
        if (!key->getNumber("date", &date))
            return nullptr;
        idbKey = IDBKey::createDate(date);
    } else if (type == array) {
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

    return idbKey.release();
}

static PassRefPtr<IDBKeyRange> idbKeyRangeFromKeyRange(JSONObject* keyRange)
{
    RefPtr<JSONObject> lower = keyRange->getObject("lower");
    RefPtr<IDBKey> idbLower = lower ? idbKeyFromInspectorObject(lower.get()) : nullptr;
    if (lower && !idbLower)
        return nullptr;

    RefPtr<JSONObject> upper = keyRange->getObject("upper");
    RefPtr<IDBKey> idbUpper = upper ? idbKeyFromInspectorObject(upper.get()) : nullptr;
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

    RefPtr<IDBKeyRange> idbKeyRange = IDBKeyRange::create(idbLower, idbUpper, lowerBoundType, upperBoundType);
    return idbKeyRange.release();
}

class DataLoader;

class OpenCursorCallback FINAL : public EventListener {
public:
    static PassRefPtr<OpenCursorCallback> create(NewScriptState* scriptState, PassRefPtr<RequestDataCallback> requestCallback, int skipCount, unsigned pageSize)
    {
        return adoptRef(new OpenCursorCallback(scriptState, requestCallback, skipCount, pageSize));
    }

    virtual ~OpenCursorCallback() { }

    virtual bool operator==(const EventListener& other) OVERRIDE
    {
        return this == &other;
    }

    virtual void handleEvent(ExecutionContext*, Event* event) OVERRIDE
    {
        if (event->type() != EventTypeNames::success) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        IDBRequest* idbRequest = static_cast<IDBRequest*>(event->target());
        RefPtr<IDBAny> requestResult = idbRequest->resultAsAny();
        if (requestResult->type() == IDBAny::BufferType) {
            end(false);
            return;
        }
        if (requestResult->type() != IDBAny::IDBCursorWithValueType) {
            m_requestCallback->sendFailure("Unexpected result type.");
            return;
        }

        RefPtr<IDBCursorWithValue> idbCursor = requestResult->idbCursorWithValue();

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
        ScriptState* scriptState = mainWorldScriptState(document->frame());

        RefPtr<DataEntry> dataEntry = DataEntry::create()
            .setKey(idbCursor->key(m_scriptState.get()).toJSONValue(scriptState)->toJSONString())
            .setPrimaryKey(idbCursor->primaryKey(m_scriptState.get()).toJSONValue(scriptState)->toJSONString())
            .setValue(idbCursor->value(m_scriptState.get()).toJSONValue(scriptState)->toJSONString());
        m_result->addItem(dataEntry);

    }

    void end(bool hasMore)
    {
        if (!m_requestCallback->isActive())
            return;
        m_requestCallback->sendSuccess(m_result.release(), hasMore);
    }

private:
    OpenCursorCallback(NewScriptState* scriptState, PassRefPtr<RequestDataCallback> requestCallback, int skipCount, unsigned pageSize)
        : EventListener(EventListener::CPPEventListenerType)
        , m_scriptState(scriptState)
        , m_requestCallback(requestCallback)
        , m_skipCount(skipCount)
        , m_pageSize(pageSize)
    {
        m_result = Array<DataEntry>::create();
    }

    RefPtr<NewScriptState> m_scriptState;
    RefPtr<RequestDataCallback> m_requestCallback;
    int m_skipCount;
    unsigned m_pageSize;
    RefPtr<Array<DataEntry> > m_result;
};

class DataLoader FINAL : public ExecutableWithDatabase {
public:
    static PassRefPtr<DataLoader> create(NewScriptState* scriptState, PassRefPtr<RequestDataCallback> requestCallback, const String& objectStoreName, const String& indexName, PassRefPtr<IDBKeyRange> idbKeyRange, int skipCount, unsigned pageSize)
    {
        return adoptRef(new DataLoader(scriptState, requestCallback, objectStoreName, indexName, idbKeyRange, skipCount, pageSize));
    }

    virtual ~DataLoader() { }

    virtual void execute(PassRefPtr<IDBDatabase> prpDatabase) OVERRIDE
    {
        RefPtr<IDBDatabase> idbDatabase = prpDatabase;
        if (!requestCallback()->isActive())
            return;
        RefPtr<IDBTransaction> idbTransaction = transactionForDatabase(context(), idbDatabase.get(), m_objectStoreName);
        if (!idbTransaction) {
            m_requestCallback->sendFailure("Could not get transaction");
            return;
        }
        RefPtr<IDBObjectStore> idbObjectStore = objectStoreForTransaction(idbTransaction.get(), m_objectStoreName);
        if (!idbObjectStore) {
            m_requestCallback->sendFailure("Could not get object store");
            return;
        }

        RefPtr<OpenCursorCallback> openCursorCallback = OpenCursorCallback::create(scriptState(), m_requestCallback, m_skipCount, m_pageSize);

        RefPtr<IDBRequest> idbRequest;
        if (!m_indexName.isEmpty()) {
            RefPtr<IDBIndex> idbIndex = indexForObjectStore(idbObjectStore.get(), m_indexName);
            if (!idbIndex) {
                m_requestCallback->sendFailure("Could not get index");
                return;
            }

            idbRequest = idbIndex->openCursor(context(), PassRefPtr<IDBKeyRange>(m_idbKeyRange), blink::WebIDBCursor::Next);
        } else {
            idbRequest = idbObjectStore->openCursor(context(), PassRefPtr<IDBKeyRange>(m_idbKeyRange), blink::WebIDBCursor::Next);
        }
        idbRequest->addEventListener(EventTypeNames::success, openCursorCallback, false);
    }

    virtual RequestCallback* requestCallback() OVERRIDE { return m_requestCallback.get(); }
    DataLoader(NewScriptState* scriptState, PassRefPtr<RequestDataCallback> requestCallback, const String& objectStoreName, const String& indexName, PassRefPtr<IDBKeyRange> idbKeyRange, int skipCount, unsigned pageSize)
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
    RefPtr<IDBKeyRange> m_idbKeyRange;
    int m_skipCount;
    unsigned m_pageSize;
};

LocalFrame* findFrameWithSecurityOrigin(Page* page, const String& securityOrigin)
{
    for (LocalFrame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr<SecurityOrigin> documentOrigin = frame->document()->securityOrigin();
        if (documentOrigin->toRawString() == securityOrigin)
            return frame;
    }
    return 0;
}

} // namespace

void InspectorIndexedDBAgent::provideTo(Page* page)
{
    OwnPtr<InspectorIndexedDBAgent> agent(adoptPtr(new InspectorIndexedDBAgent(page)));
    page->inspectorController().registerModuleAgent(agent.release());
}

InspectorIndexedDBAgent::InspectorIndexedDBAgent(Page* page)
    : InspectorBaseAgent<InspectorIndexedDBAgent>("IndexedDB")
    , m_page(page)
{
}

InspectorIndexedDBAgent::~InspectorIndexedDBAgent()
{
}

void InspectorIndexedDBAgent::clearFrontend()
{
    disable(0);
}

void InspectorIndexedDBAgent::restore()
{
    if (m_state->getBoolean(IndexedDBAgentState::indexedDBAgentEnabled)) {
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
    Document* document = frame ? frame->document() : 0;

    if (!document)
        *errorString = "No document for given frame found";

    return document;
}

static IDBFactory* assertIDBFactory(ErrorString* errorString, Document* document)
{
    DOMWindow* domWindow = document->domWindow();
    if (!domWindow) {
        *errorString = "No IndexedDB factory for given frame found";
        return 0;
    }
    IDBFactory* idbFactory = DOMWindowIndexedDatabase::indexedDB(*domWindow);

    if (!idbFactory)
        *errorString = "No IndexedDB factory for given frame found";

    return idbFactory;
}

void InspectorIndexedDBAgent::requestDatabaseNames(ErrorString* errorString, const String& securityOrigin, PassRefPtr<RequestDatabaseNamesCallback> requestCallback)
{
    LocalFrame* frame = findFrameWithSecurityOrigin(m_page, securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    NewScriptState::Scope scope(NewScriptState::forMainWorld(frame));
    TrackExceptionState exceptionState;
    RefPtr<IDBRequest> idbRequest = idbFactory->getDatabaseNames(document, exceptionState);
    if (exceptionState.hadException()) {
        requestCallback->sendFailure("Could not obtain database names.");
        return;
    }
    idbRequest->addEventListener(EventTypeNames::success, GetDatabaseNamesCallback::create(requestCallback, document->securityOrigin()->toRawString()), false);
}

void InspectorIndexedDBAgent::requestDatabase(ErrorString* errorString, const String& securityOrigin, const String& databaseName, PassRefPtr<RequestDatabaseCallback> requestCallback)
{
    LocalFrame* frame = findFrameWithSecurityOrigin(m_page, securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    NewScriptState* scriptState = NewScriptState::forMainWorld(frame);
    NewScriptState::Scope scope(scriptState);
    RefPtr<DatabaseLoader> databaseLoader = DatabaseLoader::create(scriptState, requestCallback);
    databaseLoader->start(idbFactory, document->securityOrigin(), databaseName);
}

void InspectorIndexedDBAgent::requestData(ErrorString* errorString, const String& securityOrigin, const String& databaseName, const String& objectStoreName, const String& indexName, int skipCount, int pageSize, const RefPtr<JSONObject>* keyRange, PassRefPtr<RequestDataCallback> requestCallback)
{
    LocalFrame* frame = findFrameWithSecurityOrigin(m_page, securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    RefPtr<IDBKeyRange> idbKeyRange = keyRange ? idbKeyRangeFromKeyRange(keyRange->get()) : nullptr;
    if (keyRange && !idbKeyRange) {
        *errorString = "Can not parse key range.";
        return;
    }

    NewScriptState* scriptState = NewScriptState::forMainWorld(frame);
    NewScriptState::Scope scope(scriptState);
    RefPtr<DataLoader> dataLoader = DataLoader::create(scriptState, requestCallback, objectStoreName, indexName, idbKeyRange, skipCount, pageSize);
    dataLoader->start(idbFactory, document->securityOrigin(), databaseName);
}

class ClearObjectStoreListener FINAL : public EventListener {
    WTF_MAKE_NONCOPYABLE(ClearObjectStoreListener);
public:
    static PassRefPtr<ClearObjectStoreListener> create(PassRefPtr<ClearObjectStoreCallback> requestCallback)
    {
        return adoptRef(new ClearObjectStoreListener(requestCallback));
    }

    virtual ~ClearObjectStoreListener() { }

    virtual bool operator==(const EventListener& other) OVERRIDE
    {
        return this == &other;
    }

    virtual void handleEvent(ExecutionContext*, Event* event) OVERRIDE
    {
        if (!m_requestCallback->isActive())
            return;
        if (event->type() != EventTypeNames::complete) {
            m_requestCallback->sendFailure("Unexpected event type.");
            return;
        }

        m_requestCallback->sendSuccess();
    }
private:
    ClearObjectStoreListener(PassRefPtr<ClearObjectStoreCallback> requestCallback)
        : EventListener(EventListener::CPPEventListenerType)
        , m_requestCallback(requestCallback)
    {
    }

    RefPtr<ClearObjectStoreCallback> m_requestCallback;
};


class ClearObjectStore FINAL : public ExecutableWithDatabase {
public:
    static PassRefPtr<ClearObjectStore> create(NewScriptState* scriptState, const String& objectStoreName, PassRefPtr<ClearObjectStoreCallback> requestCallback)
    {
        return adoptRef(new ClearObjectStore(scriptState, objectStoreName, requestCallback));
    }

    ClearObjectStore(NewScriptState* scriptState, const String& objectStoreName, PassRefPtr<ClearObjectStoreCallback> requestCallback)
        : ExecutableWithDatabase(scriptState)
        , m_objectStoreName(objectStoreName)
        , m_requestCallback(requestCallback)
    {
    }

    virtual void execute(PassRefPtr<IDBDatabase> prpDatabase) OVERRIDE
    {
        RefPtr<IDBDatabase> idbDatabase = prpDatabase;
        if (!requestCallback()->isActive())
            return;
        RefPtr<IDBTransaction> idbTransaction = transactionForDatabase(context(), idbDatabase.get(), m_objectStoreName, IDBTransaction::modeReadWrite());
        if (!idbTransaction) {
            m_requestCallback->sendFailure("Could not get transaction");
            return;
        }
        RefPtr<IDBObjectStore> idbObjectStore = objectStoreForTransaction(idbTransaction.get(), m_objectStoreName);
        if (!idbObjectStore) {
            m_requestCallback->sendFailure("Could not get object store");
            return;
        }

        TrackExceptionState exceptionState;
        RefPtr<IDBRequest> idbRequest = idbObjectStore->clear(context(), exceptionState);
        ASSERT(!exceptionState.hadException());
        if (exceptionState.hadException()) {
            ExceptionCode ec = exceptionState.code();
            m_requestCallback->sendFailure(String::format("Could not clear object store '%s': %d", m_objectStoreName.utf8().data(), ec));
            return;
        }
        idbTransaction->addEventListener(EventTypeNames::complete, ClearObjectStoreListener::create(m_requestCallback), false);
    }

    virtual RequestCallback* requestCallback() OVERRIDE { return m_requestCallback.get(); }
private:
    const String m_objectStoreName;
    RefPtr<ClearObjectStoreCallback> m_requestCallback;
};

void InspectorIndexedDBAgent::clearObjectStore(ErrorString* errorString, const String& securityOrigin, const String& databaseName, const String& objectStoreName, PassRefPtr<ClearObjectStoreCallback> requestCallback)
{
    LocalFrame* frame = findFrameWithSecurityOrigin(m_page, securityOrigin);
    Document* document = assertDocument(errorString, frame);
    if (!document)
        return;
    IDBFactory* idbFactory = assertIDBFactory(errorString, document);
    if (!idbFactory)
        return;

    NewScriptState* scriptState = NewScriptState::forMainWorld(frame);
    NewScriptState::Scope scope(scriptState);
    RefPtr<ClearObjectStore> clearObjectStore = ClearObjectStore::create(scriptState, objectStoreName, requestCallback);
    clearObjectStore->start(idbFactory, document->securityOrigin(), databaseName);
}

} // namespace WebCore
