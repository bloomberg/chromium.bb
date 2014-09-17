/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef DatabaseBackend_h
#define DatabaseBackend_h

#include "modules/webdatabase/DatabaseBasicTypes.h"
#include "modules/webdatabase/DatabaseError.h"
#include "modules/webdatabase/sqlite/SQLiteDatabase.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Deque.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ChangeVersionData;
class Database;
class DatabaseAuthorizer;
class DatabaseContext;
class DatabaseServer;
class ExecutionContext;
class SQLTransaction;
class SQLTransactionBackend;
class SQLTransactionClient;
class SQLTransactionCoordinator;

// FIXME: This implementation of DatabaseBackend is only a place holder
// for the split out of the Database backend to be done later. This
// place holder is needed to allow other code that need to reference the
// DatabaseBackend to do so before the proper backend split is
// available. This should be replaced with the actual implementation later.

class DatabaseBackend : public ThreadSafeRefCountedWillBeGarbageCollectedFinalized<DatabaseBackend> {
public:
    virtual ~DatabaseBackend();
    virtual void trace(Visitor*);

    bool openAndVerifyVersion(bool setVersionInNewDatabase, DatabaseError&, String& errorMessage);
    void close();

    PassRefPtrWillBeRawPtr<SQLTransactionBackend> runTransaction(PassRefPtrWillBeRawPtr<SQLTransaction>, bool readOnly, const ChangeVersionData*);
    void scheduleTransactionStep(SQLTransactionBackend*);
    void inProgressTransactionCompleted();

    SQLTransactionClient* transactionClient() const;
    SQLTransactionCoordinator* transactionCoordinator() const;

    virtual String version() const;

    bool opened() const { return m_opened; }
    bool isNew() const { return m_new; }

    virtual SecurityOrigin* securityOrigin() const;
    virtual String stringIdentifier() const;
    virtual String displayName() const;
    virtual unsigned long estimatedSize() const;
    virtual String fileName() const;
    SQLiteDatabase& sqliteDatabase() { return m_sqliteDatabase; }

    unsigned long long maximumSize() const;
    void incrementalVacuumIfNeeded();

    void disableAuthorizer();
    void enableAuthorizer();
    void setAuthorizerPermissions(int);
    bool lastActionChangedDatabase();
    bool lastActionWasInsert();
    void resetDeletes();
    bool hadDeletes();
    void resetAuthorizer();

    virtual void closeImmediately() = 0;
    void closeDatabase();

    DatabaseContext* databaseContext() const { return m_databaseContext.get(); }
    ExecutionContext* executionContext() const;
    void setFrontend(Database* frontend) { m_frontend = frontend; }

protected:
    DatabaseBackend(DatabaseContext*, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize);

private:
    class DatabaseOpenTask;
    class DatabaseCloseTask;
    class DatabaseTransactionTask;
    class DatabaseTableNamesTask;

    bool performOpenAndVerify(bool setVersionInNewDatabase, DatabaseError&, String& errorMessage);

    void scheduleTransaction();

    bool getVersionFromDatabase(String& version, bool shouldCacheVersion = true);
    bool setVersionInDatabase(const String& version, bool shouldCacheVersion = true);
    void setExpectedVersion(const String&);
    const String& expectedVersion() const { return m_expectedVersion; }
    String getCachedVersion()const;
    void setCachedVersion(const String&);
    bool getActualVersionForTransaction(String& version);

    void reportOpenDatabaseResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode);
    void reportChangeVersionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode);
    void reportStartTransactionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode);
    void reportCommitTransactionResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode);
    void reportExecuteStatementResult(int errorSite, int webSqlErrorCode, int sqliteErrorCode);
    void reportVacuumDatabaseResult(int sqliteErrorCode);
    void logErrorMessage(const String&);
    static const char* databaseInfoTableName();
#if !LOG_DISABLED || !ERROR_DISABLED
    String databaseDebugName() const { return m_contextThreadSecurityOrigin->toString() + "::" + m_name; }
#endif

    RefPtr<SecurityOrigin> m_contextThreadSecurityOrigin;
    RefPtrWillBeMember<DatabaseContext> m_databaseContext; // Associated with m_executionContext.

    String m_name;
    String m_expectedVersion;
    String m_displayName;
    unsigned long m_estimatedSize;
    String m_filename;

    Database* m_frontend;

    DatabaseGuid m_guid;
    bool m_opened;
    bool m_new;

    SQLiteDatabase m_sqliteDatabase;

    RefPtrWillBeMember<DatabaseAuthorizer> m_databaseAuthorizer;

    Deque<RefPtrWillBeMember<SQLTransactionBackend> > m_transactionQueue;
    Mutex m_transactionInProgressMutex;
    bool m_transactionInProgress;
    bool m_isTransactionQueueEnabled;

    friend class ChangeVersionWrapper;
    friend class Database;
    friend class SQLStatementBackend;
    friend class SQLTransactionBackend;
};

} // namespace blink

#endif // DatabaseBackend_h
