/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/webdatabase/DatabaseManager.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/inspector/InspectorDatabaseInstrumentation.h"
#include "platform/Logging.h"
#include "modules/webdatabase/AbstractDatabaseServer.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseBackend.h"
#include "modules/webdatabase/DatabaseBackendBase.h"
#include "modules/webdatabase/DatabaseBackendContext.h"
#include "modules/webdatabase/DatabaseBackendSync.h"
#include "modules/webdatabase/DatabaseCallback.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseServer.h"
#include "modules/webdatabase/DatabaseSync.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "weborigin/SecurityOrigin.h"

namespace WebCore {

DatabaseManager& DatabaseManager::manager()
{
    static DatabaseManager* dbManager = 0;
    // FIXME: The following is vulnerable to a race between threads. Need to
    // implement a thread safe on-first-use static initializer.
    if (!dbManager)
        dbManager = new DatabaseManager();

    return *dbManager;
}

DatabaseManager::DatabaseManager()
#if !ASSERT_DISABLED
    : m_databaseContextRegisteredCount(0)
    , m_databaseContextInstanceCount(0)
#endif
{
    m_server = new DatabaseServer;
    ASSERT(m_server); // We should always have a server to work with.
}

class DatabaseCreationCallbackTask : public ScriptExecutionContext::Task {
public:
    static PassOwnPtr<DatabaseCreationCallbackTask> create(PassRefPtr<Database> database, PassRefPtr<DatabaseCallback> creationCallback)
    {
        return adoptPtr(new DatabaseCreationCallbackTask(database, creationCallback));
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        m_creationCallback->handleEvent(m_database.get());
    }

private:
    DatabaseCreationCallbackTask(PassRefPtr<Database> database, PassRefPtr<DatabaseCallback> callback)
        : m_database(database)
        , m_creationCallback(callback)
    {
    }

    RefPtr<Database> m_database;
    RefPtr<DatabaseCallback> m_creationCallback;
};

PassRefPtr<DatabaseContext> DatabaseManager::existingDatabaseContextFor(ScriptExecutionContext* context)
{
    MutexLocker locker(m_contextMapLock);

    ASSERT(m_databaseContextRegisteredCount >= 0);
    ASSERT(m_databaseContextInstanceCount >= 0);
    ASSERT(m_databaseContextRegisteredCount <= m_databaseContextInstanceCount);

    RefPtr<DatabaseContext> databaseContext = adoptRef(m_contextMap.get(context));
    if (databaseContext) {
        // If we're instantiating a new DatabaseContext, the new instance would
        // carry a new refCount of 1. The client expects this and will simply
        // adoptRef the databaseContext without ref'ing it.
        //     However, instead of instantiating a new instance, we're reusing
        // an existing one that corresponds to the specified ScriptExecutionContext.
        // Hence, that new refCount need to be attributed to the reused instance
        // to ensure that the refCount is accurate when the client adopts the ref.
        // We do this by ref'ing the reused databaseContext before returning it.
        databaseContext->ref();
    }
    return databaseContext.release();
}

PassRefPtr<DatabaseContext> DatabaseManager::databaseContextFor(ScriptExecutionContext* context)
{
    RefPtr<DatabaseContext> databaseContext = existingDatabaseContextFor(context);
    if (!databaseContext)
        databaseContext = adoptRef(new DatabaseBackendContext(context));
    return databaseContext.release();
}

void DatabaseManager::registerDatabaseContext(DatabaseContext* databaseContext)
{
    MutexLocker locker(m_contextMapLock);
    ScriptExecutionContext* context = databaseContext->scriptExecutionContext();
    m_contextMap.set(context, databaseContext);
#if !ASSERT_DISABLED
    m_databaseContextRegisteredCount++;
#endif
}

void DatabaseManager::unregisterDatabaseContext(DatabaseContext* databaseContext)
{
    MutexLocker locker(m_contextMapLock);
    ScriptExecutionContext* context = databaseContext->scriptExecutionContext();
    ASSERT(m_contextMap.get(context));
#if !ASSERT_DISABLED
    m_databaseContextRegisteredCount--;
#endif
    m_contextMap.remove(context);
}

#if !ASSERT_DISABLED
void DatabaseManager::didConstructDatabaseContext()
{
    MutexLocker lock(m_contextMapLock);
    m_databaseContextInstanceCount++;
}

void DatabaseManager::didDestructDatabaseContext()
{
    MutexLocker lock(m_contextMapLock);
    m_databaseContextInstanceCount--;
    ASSERT(m_databaseContextRegisteredCount <= m_databaseContextInstanceCount);
}
#endif

void DatabaseManager::throwExceptionForDatabaseError(const String& method, const String& context, DatabaseError error, const String& errorMessage, ExceptionState& es)
{
    switch (error) {
    case DatabaseError::None:
        return;
    case DatabaseError::GenericSecurityError:
        es.throwSecurityError(ExceptionMessages::failedToExecute(method, context, errorMessage));
        return;
    case DatabaseError::InvalidDatabaseState:
        es.throwDOMException(InvalidStateError, ExceptionMessages::failedToExecute(method, context, errorMessage));
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

static void logOpenDatabaseError(ScriptExecutionContext* context, const String& name)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(name);
    LOG(StorageAPI, "Database %s for origin %s not allowed to be established", name.ascii().data(),
        context->securityOrigin()->toString().ascii().data());
}

PassRefPtr<DatabaseBackendBase> DatabaseManager::openDatabaseBackend(ScriptExecutionContext* context,
    DatabaseType type, const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, bool setVersionInNewDatabase, DatabaseError& error, String& errorMessage)
{
    ASSERT(error == DatabaseError::None);

    RefPtr<DatabaseContext> databaseContext = databaseContextFor(context);
    RefPtr<DatabaseBackendContext> backendContext = databaseContext->backend();

    RefPtr<DatabaseBackendBase> backend = m_server->openDatabase(backendContext, type, name, expectedVersion,
        displayName, estimatedSize, setVersionInNewDatabase, error, errorMessage);

    if (!backend) {
        ASSERT(error != DatabaseError::None);

        switch (error) {
        case DatabaseError::GenericSecurityError:
            logOpenDatabaseError(context, name);
            return 0;

        case DatabaseError::InvalidDatabaseState:
            logErrorMessage(context, errorMessage);
            return 0;

        default:
            ASSERT_NOT_REACHED();
        }
    }

    return backend.release();
}

PassRefPtr<Database> DatabaseManager::openDatabase(ScriptExecutionContext* context,
    const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback,
    DatabaseError& error, String& errorMessage)
{
    ASSERT(error == DatabaseError::None);

    bool setVersionInNewDatabase = !creationCallback;
    RefPtr<DatabaseBackendBase> backend = openDatabaseBackend(context, DatabaseType::Async, name,
        expectedVersion, displayName, estimatedSize, setVersionInNewDatabase, error, errorMessage);
    if (!backend)
        return 0;

    RefPtr<Database> database = Database::create(context, backend);

    RefPtr<DatabaseContext> databaseContext = databaseContextFor(context);
    databaseContext->setHasOpenDatabases();
    InspectorInstrumentation::didOpenDatabase(context, database, context->securityOrigin()->host(), name, expectedVersion);

    if (backend->isNew() && creationCallback.get()) {
        LOG(StorageAPI, "Scheduling DatabaseCreationCallbackTask for database %p\n", database.get());
        database->m_scriptExecutionContext->postTask(DatabaseCreationCallbackTask::create(database, creationCallback));
    }

    ASSERT(database);
    return database.release();
}

PassRefPtr<DatabaseSync> DatabaseManager::openDatabaseSync(ScriptExecutionContext* context,
    const String& name, const String& expectedVersion, const String& displayName,
    unsigned long estimatedSize, PassRefPtr<DatabaseCallback> creationCallback,
    DatabaseError& error, String& errorMessage)
{
    ASSERT(context->isContextThread());
    ASSERT(error == DatabaseError::None);

    bool setVersionInNewDatabase = !creationCallback;
    RefPtr<DatabaseBackendBase> backend = openDatabaseBackend(context, DatabaseType::Sync, name,
        expectedVersion, displayName, estimatedSize, setVersionInNewDatabase, error, errorMessage);
    if (!backend)
        return 0;

    RefPtr<DatabaseSync> database = DatabaseSync::create(context, backend);

    if (backend->isNew() && creationCallback.get()) {
        LOG(StorageAPI, "Invoking the creation callback for database %p\n", database.get());
        creationCallback->handleEvent(database.get());
    }

    ASSERT(database);
    return database.release();
}

bool DatabaseManager::hasOpenDatabases(ScriptExecutionContext* context)
{
    RefPtr<DatabaseContext> databaseContext = existingDatabaseContextFor(context);
    if (!databaseContext)
        return false;
    return databaseContext->hasOpenDatabases();
}

void DatabaseManager::stopDatabases(ScriptExecutionContext* context, DatabaseTaskSynchronizer* synchronizer)
{
    RefPtr<DatabaseContext> databaseContext = existingDatabaseContextFor(context);
    if (!databaseContext || !databaseContext->stopDatabases(synchronizer))
        if (synchronizer)
            synchronizer->taskCompleted();
}

String DatabaseManager::fullPathForDatabase(SecurityOrigin* origin, const String& name, bool createIfDoesNotExist)
{
    return m_server->fullPathForDatabase(origin, name, createIfDoesNotExist);
}

void DatabaseManager::closeDatabasesImmediately(const String& originIdentifier, const String& name)
{
    m_server->closeDatabasesImmediately(originIdentifier, name);
}

void DatabaseManager::interruptAllDatabasesForContext(ScriptExecutionContext* context)
{
    RefPtr<DatabaseContext> databaseContext = existingDatabaseContextFor(context);
    if (databaseContext)
        m_server->interruptAllDatabasesForContext(databaseContext->backend().get());
}

void DatabaseManager::logErrorMessage(ScriptExecutionContext* context, const String& message)
{
    context->addConsoleMessage(StorageMessageSource, ErrorMessageLevel, message);
}

} // namespace WebCore
