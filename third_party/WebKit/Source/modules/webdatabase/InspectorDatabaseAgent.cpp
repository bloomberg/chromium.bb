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

#include "config.h"
#include "modules/webdatabase/InspectorDatabaseAgent.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/frame/LocalFrame.h"
#include "core/html/VoidCallback.h"
#include "core/inspector/InspectorState.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "modules/webdatabase/DatabaseBackend.h"
#include "modules/webdatabase/InspectorDatabaseResource.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLResultSet.h"
#include "modules/webdatabase/SQLResultSetRowList.h"
#include "modules/webdatabase/SQLStatementCallback.h"
#include "modules/webdatabase/SQLStatementErrorCallback.h"
#include "modules/webdatabase/SQLTransaction.h"
#include "modules/webdatabase/SQLTransactionCallback.h"
#include "modules/webdatabase/SQLTransactionErrorCallback.h"
#include "modules/webdatabase/sqlite/SQLValue.h"
#include "platform/JSONValues.h"
#include "wtf/Vector.h"

typedef blink::InspectorBackendDispatcher::DatabaseCommandHandler::ExecuteSQLCallback ExecuteSQLCallback;

namespace blink {

namespace DatabaseAgentState {
static const char databaseAgentEnabled[] = "databaseAgentEnabled";
};

namespace {

void reportTransactionFailed(ExecuteSQLCallback* requestCallback, SQLError* error)
{
    RefPtr<TypeBuilder::Database::Error> errorObject = TypeBuilder::Database::Error::create()
        .setMessage(error->message())
        .setCode(error->code());
    requestCallback->sendSuccess(nullptr, nullptr, errorObject.release());
}

class StatementCallback FINAL : public SQLStatementCallback {
public:
    static PassOwnPtrWillBeRawPtr<StatementCallback> create(PassRefPtrWillBeRawPtr<ExecuteSQLCallback> requestCallback)
    {
        return adoptPtrWillBeNoop(new StatementCallback(requestCallback));
    }

    virtual ~StatementCallback() { }

    virtual void trace(Visitor* visitor)
    {
        visitor->trace(m_requestCallback);
        SQLStatementCallback::trace(visitor);
    }

    virtual bool handleEvent(SQLTransaction*, SQLResultSet* resultSet) OVERRIDE
    {
        SQLResultSetRowList* rowList = resultSet->rows();

        RefPtr<TypeBuilder::Array<String> > columnNames = TypeBuilder::Array<String>::create();
        const Vector<String>& columns = rowList->columnNames();
        for (size_t i = 0; i < columns.size(); ++i)
            columnNames->addItem(columns[i]);

        RefPtr<TypeBuilder::Array<JSONValue> > values = TypeBuilder::Array<JSONValue>::create();
        const Vector<SQLValue>& data = rowList->values();
        for (size_t i = 0; i < data.size(); ++i) {
            const SQLValue& value = rowList->values()[i];
            switch (value.type()) {
            case SQLValue::StringValue: values->addItem(JSONString::create(value.string())); break;
            case SQLValue::NumberValue: values->addItem(JSONBasicValue::create(value.number())); break;
            case SQLValue::NullValue: values->addItem(JSONValue::null()); break;
            }
        }
        m_requestCallback->sendSuccess(columnNames.release(), values.release(), nullptr);
        return true;
    }

private:
    StatementCallback(PassRefPtrWillBeRawPtr<ExecuteSQLCallback> requestCallback)
        : m_requestCallback(requestCallback) { }
    RefPtrWillBeMember<ExecuteSQLCallback> m_requestCallback;
};

class StatementErrorCallback FINAL : public SQLStatementErrorCallback {
public:
    static PassOwnPtrWillBeRawPtr<StatementErrorCallback> create(PassRefPtrWillBeRawPtr<ExecuteSQLCallback> requestCallback)
    {
        return adoptPtrWillBeNoop(new StatementErrorCallback(requestCallback));
    }

    virtual ~StatementErrorCallback() { }

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        visitor->trace(m_requestCallback);
        SQLStatementErrorCallback::trace(visitor);
    }

    virtual bool handleEvent(SQLTransaction*, SQLError* error) OVERRIDE
    {
        reportTransactionFailed(m_requestCallback.get(), error);
        return true;
    }

private:
    StatementErrorCallback(PassRefPtrWillBeRawPtr<ExecuteSQLCallback> requestCallback)
        : m_requestCallback(requestCallback) { }
    RefPtrWillBeMember<ExecuteSQLCallback> m_requestCallback;
};

class TransactionCallback FINAL : public SQLTransactionCallback {
public:
    static PassOwnPtrWillBeRawPtr<TransactionCallback> create(const String& sqlStatement, PassRefPtrWillBeRawPtr<ExecuteSQLCallback> requestCallback)
    {
        return adoptPtrWillBeNoop(new TransactionCallback(sqlStatement, requestCallback));
    }

    virtual ~TransactionCallback() { }

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        visitor->trace(m_requestCallback);
        SQLTransactionCallback::trace(visitor);
    }

    virtual bool handleEvent(SQLTransaction* transaction) OVERRIDE
    {
        if (!m_requestCallback->isActive())
            return true;

        Vector<SQLValue> sqlValues;
        OwnPtrWillBeRawPtr<SQLStatementCallback> callback(StatementCallback::create(m_requestCallback.get()));
        OwnPtrWillBeRawPtr<SQLStatementErrorCallback> errorCallback(StatementErrorCallback::create(m_requestCallback.get()));
        transaction->executeSQL(m_sqlStatement, sqlValues, callback.release(), errorCallback.release(), IGNORE_EXCEPTION);
        return true;
    }
private:
    TransactionCallback(const String& sqlStatement, PassRefPtrWillBeRawPtr<ExecuteSQLCallback> requestCallback)
        : m_sqlStatement(sqlStatement)
        , m_requestCallback(requestCallback) { }
    String m_sqlStatement;
    RefPtrWillBeMember<ExecuteSQLCallback> m_requestCallback;
};

class TransactionErrorCallback FINAL : public SQLTransactionErrorCallback {
public:
    static PassOwnPtrWillBeRawPtr<TransactionErrorCallback> create(PassRefPtrWillBeRawPtr<ExecuteSQLCallback> requestCallback)
    {
        return adoptPtrWillBeNoop(new TransactionErrorCallback(requestCallback));
    }

    virtual ~TransactionErrorCallback() { }

    virtual void trace(Visitor* visitor) OVERRIDE
    {
        visitor->trace(m_requestCallback);
        SQLTransactionErrorCallback::trace(visitor);
    }

    virtual bool handleEvent(SQLError* error) OVERRIDE
    {
        reportTransactionFailed(m_requestCallback.get(), error);
        return true;
    }
private:
    TransactionErrorCallback(PassRefPtrWillBeRawPtr<ExecuteSQLCallback> requestCallback)
        : m_requestCallback(requestCallback) { }
    RefPtrWillBeMember<ExecuteSQLCallback> m_requestCallback;
};

class TransactionSuccessCallback FINAL : public VoidCallback {
public:
    static PassOwnPtrWillBeRawPtr<TransactionSuccessCallback> create()
    {
        return adoptPtrWillBeNoop(new TransactionSuccessCallback());
    }

    virtual ~TransactionSuccessCallback() { }

    virtual void handleEvent() OVERRIDE { }

private:
    TransactionSuccessCallback() { }
};

} // namespace

void InspectorDatabaseAgent::didOpenDatabase(PassRefPtrWillBeRawPtr<Database> database, const String& domain, const String& name, const String& version)
{
    if (InspectorDatabaseResource* resource = findByFileName(database->fileName())) {
        resource->setDatabase(database);
        return;
    }

    RefPtrWillBeRawPtr<InspectorDatabaseResource> resource = InspectorDatabaseResource::create(database, domain, name, version);
    m_resources.set(resource->id(), resource);
    // Resources are only bound while visible.
    if (m_frontend && m_enabled)
        resource->bind(m_frontend);
}

void InspectorDatabaseAgent::didCommitLoadForMainFrame()
{
    m_resources.clear();
}

InspectorDatabaseAgent::InspectorDatabaseAgent()
    : InspectorBaseAgent<InspectorDatabaseAgent>("Database")
    , m_frontend(0)
    , m_enabled(false)
{
}

InspectorDatabaseAgent::~InspectorDatabaseAgent()
{
}

void InspectorDatabaseAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->database();
}

void InspectorDatabaseAgent::clearFrontend()
{
    m_frontend = 0;
    disable(0);
}

void InspectorDatabaseAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
    m_state->setBoolean(DatabaseAgentState::databaseAgentEnabled, m_enabled);

    DatabaseResourcesHeapMap::iterator databasesEnd = m_resources.end();
    for (DatabaseResourcesHeapMap::iterator it = m_resources.begin(); it != databasesEnd; ++it)
        it->value->bind(m_frontend);
}

void InspectorDatabaseAgent::disable(ErrorString*)
{
    if (!m_enabled)
        return;
    m_enabled = false;
    m_state->setBoolean(DatabaseAgentState::databaseAgentEnabled, m_enabled);
}

void InspectorDatabaseAgent::restore()
{
    m_enabled = m_state->getBoolean(DatabaseAgentState::databaseAgentEnabled);
}

void InspectorDatabaseAgent::getDatabaseTableNames(ErrorString* error, const String& databaseId, RefPtr<TypeBuilder::Array<String> >& names)
{
    if (!m_enabled) {
        *error = "Database agent is not enabled";
        return;
    }

    names = TypeBuilder::Array<String>::create();

    Database* database = databaseForId(databaseId);
    if (database) {
        Vector<String> tableNames = database->tableNames();
        unsigned length = tableNames.size();
        for (unsigned i = 0; i < length; ++i)
            names->addItem(tableNames[i]);
    }
}

void InspectorDatabaseAgent::executeSQL(ErrorString*, const String& databaseId, const String& query, PassRefPtrWillBeRawPtr<ExecuteSQLCallback> prpRequestCallback)
{
    RefPtrWillBeRawPtr<ExecuteSQLCallback> requestCallback = prpRequestCallback;

    if (!m_enabled) {
        requestCallback->sendFailure("Database agent is not enabled");
        return;
    }

    Database* database = databaseForId(databaseId);
    if (!database) {
        requestCallback->sendFailure("Database not found");
        return;
    }

    OwnPtrWillBeRawPtr<SQLTransactionCallback> callback(TransactionCallback::create(query, requestCallback.get()));
    OwnPtrWillBeRawPtr<SQLTransactionErrorCallback> errorCallback(TransactionErrorCallback::create(requestCallback.get()));
    OwnPtrWillBeRawPtr<VoidCallback> successCallback(TransactionSuccessCallback::create());
    database->transaction(callback.release(), errorCallback.release(), successCallback.release());
}

InspectorDatabaseResource* InspectorDatabaseAgent::findByFileName(const String& fileName)
{
    for (DatabaseResourcesHeapMap::iterator it = m_resources.begin(); it != m_resources.end(); ++it) {
        if (it->value->database()->fileName() == fileName)
            return it->value.get();
    }
    return 0;
}

Database* InspectorDatabaseAgent::databaseForId(const String& databaseId)
{
    DatabaseResourcesHeapMap::iterator it = m_resources.find(databaseId);
    if (it == m_resources.end())
        return 0;
    return it->value->database();
}

void InspectorDatabaseAgent::trace(Visitor* visitor)
{
#if ENABLE(OILPAN)
    visitor->trace(m_resources);
#endif
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink
