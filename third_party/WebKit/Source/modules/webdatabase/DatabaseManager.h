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

#ifndef DatabaseManager_h
#define DatabaseManager_h

#include "modules/webdatabase/DatabaseBasicTypes.h"
#include "modules/webdatabase/DatabaseDetails.h"
#include "modules/webdatabase/DatabaseError.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/ThreadingPrimitives.h"

namespace WebCore {

class AbstractDatabaseServer;
class Database;
class DatabaseBackendBase;
class DatabaseCallback;
class DatabaseContext;
class DatabaseSync;
class DatabaseTaskSynchronizer;
class ExceptionState;
class SecurityOrigin;
class ScriptExecutionContext;

typedef int ExceptionCode;

class DatabaseManager {
    WTF_MAKE_NONCOPYABLE(DatabaseManager); WTF_MAKE_FAST_ALLOCATED;
public:
    static DatabaseManager& manager();

    // This gets a DatabaseContext for the specified ScriptExecutionContext.
    // If one doesn't already exist, it will create a new one.
    PassRefPtr<DatabaseContext> databaseContextFor(ScriptExecutionContext*);

    // These 2 methods are for DatabaseContext (un)registration, and should only
    // be called by the DatabaseContext constructor and destructor.
    void registerDatabaseContext(DatabaseContext*);
    void unregisterDatabaseContext(DatabaseContext*);

#if !ASSERT_DISABLED
    void didConstructDatabaseContext();
    void didDestructDatabaseContext();
#else
    void didConstructDatabaseContext() { }
    void didDestructDatabaseContext() { }
#endif

    static void throwExceptionForDatabaseError(const String& method, const String& context, DatabaseError, const String& errorMessage, ExceptionState&);

    PassRefPtr<Database> openDatabase(ScriptExecutionContext*, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback>, DatabaseError&, String& errorMessage);
    PassRefPtr<DatabaseSync> openDatabaseSync(ScriptExecutionContext*, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize, PassRefPtr<DatabaseCallback>, DatabaseError&, String& errorMessage);

    bool hasOpenDatabases(ScriptExecutionContext*);
    void stopDatabases(ScriptExecutionContext*, DatabaseTaskSynchronizer*);

    String fullPathForDatabase(SecurityOrigin*, const String& name, bool createIfDoesNotExist = true);

    void closeDatabasesImmediately(const String& originIdentifier, const String& name);

    void interruptAllDatabasesForContext(ScriptExecutionContext*);

private:
    DatabaseManager();
    ~DatabaseManager() { }

    // This gets a DatabaseContext for the specified ScriptExecutionContext if
    // it already exist previously. Otherwise, it returns 0.
    PassRefPtr<DatabaseContext> existingDatabaseContextFor(ScriptExecutionContext*);

    PassRefPtr<DatabaseBackendBase> openDatabaseBackend(ScriptExecutionContext*,
        DatabaseType, const String& name, const String& expectedVersion, const String& displayName,
        unsigned long estimatedSize, bool setVersionInNewDatabase, DatabaseError&, String& errorMessage);

    static void logErrorMessage(ScriptExecutionContext*, const String& message);

    AbstractDatabaseServer* m_server;

    // Access to the following fields require locking m_contextMapLock:
    typedef HashMap<ScriptExecutionContext*, DatabaseContext*> ContextMap;
    ContextMap m_contextMap;
#if !ASSERT_DISABLED
    int m_databaseContextRegisteredCount;
    int m_databaseContextInstanceCount;
#endif
    Mutex m_contextMapLock;
};

} // namespace WebCore

#endif // DatabaseManager_h
