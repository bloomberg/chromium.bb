/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "modules/webdatabase/DatabaseSync.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ScriptExecutionContext.h"
#include "platform/Logging.h"
#include "modules/webdatabase/DatabaseBackendContext.h"
#include "modules/webdatabase/DatabaseBackendSync.h"
#include "modules/webdatabase/DatabaseCallback.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webdatabase/DatabaseTracker.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLTransactionSync.h"
#include "modules/webdatabase/SQLTransactionSyncCallback.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/CString.h"

namespace WebCore {

PassRefPtr<DatabaseSync> DatabaseSync::create(ScriptExecutionContext*, PassRefPtr<DatabaseBackendBase> backend)
{
    return static_cast<DatabaseSync*>(backend.get());
}

DatabaseSync::DatabaseSync(PassRefPtr<DatabaseBackendContext> databaseContext,
    const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize)
    : DatabaseBase(databaseContext->scriptExecutionContext())
    , DatabaseBackendSync(databaseContext, name, expectedVersion, displayName, estimatedSize)
{
    ScriptWrappable::init(this);
    setFrontend(this);
}

DatabaseSync::~DatabaseSync()
{
    ASSERT(m_scriptExecutionContext->isContextThread());
}

PassRefPtr<DatabaseBackendSync> DatabaseSync::backend()
{
    return this;
}

void DatabaseSync::changeVersion(const String& oldVersion, const String& newVersion, PassRefPtr<SQLTransactionSyncCallback> changeVersionCallback, ExceptionState& es)
{
    ASSERT(m_scriptExecutionContext->isContextThread());

    if (sqliteDatabase().transactionInProgress()) {
        reportChangeVersionResult(1, SQLError::DATABASE_ERR, 0);
        setLastErrorMessage("unable to changeVersion from within a transaction");
        es.throwUninformativeAndGenericDOMException(SQLDatabaseError);
        return;
    }

    RefPtr<SQLTransactionSync> transaction = SQLTransactionSync::create(this, changeVersionCallback, false);
    transaction->begin(es);
    if (es.hadException()) {
        ASSERT(!lastErrorMessage().isEmpty());
        return;
    }

    String actualVersion;
    if (!getVersionFromDatabase(actualVersion)) {
        reportChangeVersionResult(2, SQLError::UNKNOWN_ERR, sqliteDatabase().lastError());
        setLastErrorMessage("unable to read the current version", sqliteDatabase().lastError(), sqliteDatabase().lastErrorMsg());
        es.throwDOMException(UnknownError, SQLError::unknownErrorMessage);
        return;
    }

    if (actualVersion != oldVersion) {
        reportChangeVersionResult(3, SQLError::VERSION_ERR, 0);
        setLastErrorMessage("current version of the database and `oldVersion` argument do not match");
        es.throwDOMException(VersionError, SQLError::versionErrorMessage);
        return;
    }


    transaction->execute(es);
    if (es.hadException()) {
        ASSERT(!lastErrorMessage().isEmpty());
        return;
    }

    if (!setVersionInDatabase(newVersion)) {
        reportChangeVersionResult(4, SQLError::UNKNOWN_ERR, sqliteDatabase().lastError());
        setLastErrorMessage("unable to set the new version", sqliteDatabase().lastError(), sqliteDatabase().lastErrorMsg());
        es.throwDOMException(UnknownError, SQLError::unknownErrorMessage);
        return;
    }

    transaction->commit(es);
    if (es.hadException()) {
        ASSERT(!lastErrorMessage().isEmpty());
        setCachedVersion(oldVersion);
        return;
    }

    reportChangeVersionResult(0, -1, 0); // OK

    setExpectedVersion(newVersion);
    setLastErrorMessage("");
}

void DatabaseSync::transaction(PassRefPtr<SQLTransactionSyncCallback> callback, ExceptionState& es)
{
    runTransaction(callback, false, es);
}

void DatabaseSync::readTransaction(PassRefPtr<SQLTransactionSyncCallback> callback, ExceptionState& es)
{
    runTransaction(callback, true, es);
}

void DatabaseSync::rollbackTransaction(PassRefPtr<SQLTransactionSync> transaction)
{
    ASSERT(!lastErrorMessage().isEmpty());
    transaction->rollback();
    setLastErrorMessage("");
    return;
}

void DatabaseSync::runTransaction(PassRefPtr<SQLTransactionSyncCallback> callback, bool readOnly, ExceptionState& es)
{
    ASSERT(m_scriptExecutionContext->isContextThread());

    if (sqliteDatabase().transactionInProgress()) {
        setLastErrorMessage("unable to start a transaction from within a transaction");
        es.throwUninformativeAndGenericDOMException(SQLDatabaseError);
        return;
    }

    RefPtr<SQLTransactionSync> transaction = SQLTransactionSync::create(this, callback, readOnly);
    transaction->begin(es);
    if (es.hadException()) {
        rollbackTransaction(transaction);
        return;
    }

    transaction->execute(es);
    if (es.hadException()) {
        rollbackTransaction(transaction);
        return;
    }

    transaction->commit(es);
    if (es.hadException()) {
        rollbackTransaction(transaction);
        return;
    }

    setLastErrorMessage("");
}

void DatabaseSync::markAsDeletedAndClose()
{
    // FIXME: need to do something similar to closeImmediately(), but in a sync way
}

class CloseSyncDatabaseOnContextThreadTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<CloseSyncDatabaseOnContextThreadTask> create(PassRefPtr<DatabaseSync> database)
    {
        return adoptPtr(new CloseSyncDatabaseOnContextThreadTask(database));
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_database->closeImmediately();
    }

private:
    CloseSyncDatabaseOnContextThreadTask(PassRefPtr<DatabaseSync> database)
        : m_database(database)
    {
    }

    RefPtr<DatabaseSync> m_database;
};

void DatabaseSync::closeImmediately()
{
    ASSERT(m_scriptExecutionContext->isContextThread());

    if (!opened())
        return;

    logErrorMessage("forcibly closing database");
    closeDatabase();
}

} // namespace WebCore
