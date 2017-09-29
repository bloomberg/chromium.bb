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

#ifndef Database_h
#define Database_h

#include "modules/webdatabase/DatabaseBasicTypes.h"
#include "modules/webdatabase/DatabaseError.h"
#include "modules/webdatabase/sqlite/SQLiteDatabase.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ChangeVersionData;
class DatabaseAuthorizer;
class DatabaseContext;
class ExecutionContext;
class SQLTransaction;
class SQLTransactionBackend;
class SQLTransactionCallback;
class SQLTransactionClient;
class SQLTransactionCoordinator;
class SQLTransactionErrorCallback;
class V8DatabaseCallback;
class VoidCallback;

class Database final : public GarbageCollectedFinalized<Database>,
                       public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~Database();
  DECLARE_TRACE();
  DECLARE_TRACE_WRAPPERS();

  bool OpenAndVerifyVersion(bool set_version_in_new_database,
                            DatabaseError&,
                            String& error_message);
  void Close();

  SQLTransactionBackend* RunTransaction(SQLTransaction*,
                                        bool read_only,
                                        const ChangeVersionData*);
  void ScheduleTransactionStep(SQLTransactionBackend*);
  void InProgressTransactionCompleted();

  SQLTransactionClient* TransactionClient() const;
  SQLTransactionCoordinator* TransactionCoordinator() const;

  // Direct support for the DOM API
  String version() const;
  void changeVersion(const String& old_version,
                     const String& new_version,
                     SQLTransactionCallback*,
                     SQLTransactionErrorCallback*,
                     VoidCallback* success_callback);
  void transaction(SQLTransactionCallback*,
                   SQLTransactionErrorCallback*,
                   VoidCallback* success_callback);
  void readTransaction(SQLTransactionCallback*,
                       SQLTransactionErrorCallback*,
                       VoidCallback* success_callback);

  bool Opened();
  bool IsNew() const { return new_; }

  SecurityOrigin* GetSecurityOrigin() const;
  String StringIdentifier() const;
  String DisplayName() const;
  unsigned EstimatedSize() const;
  String FileName() const;
  SQLiteDatabase& SqliteDatabase() { return sqlite_database_; }

  unsigned long long MaximumSize() const;
  void IncrementalVacuumIfNeeded();

  void DisableAuthorizer();
  void EnableAuthorizer();
  void SetAuthorizerPermissions(int);
  bool LastActionChangedDatabase();
  bool LastActionWasInsert();
  void ResetDeletes();
  bool HadDeletes();
  void ResetAuthorizer();

  Vector<String> TableNames();
  void ScheduleTransactionCallback(SQLTransaction*);
  void CloseImmediately();
  void CloseDatabase();

  DatabaseContext* GetDatabaseContext() const {
    return database_context_.Get();
  }
  ExecutionContext* GetExecutionContext() const;
  WebTaskRunner* GetDatabaseTaskRunner() const;

 private:
  class DatabaseOpenTask;
  class DatabaseCloseTask;
  class DatabaseTransactionTask;
  class DatabaseTableNamesTask;

  Database(DatabaseContext*,
           const String& name,
           const String& expected_version,
           const String& display_name,
           unsigned estimated_size,
           V8DatabaseCallback* creation_callback);
  bool PerformOpenAndVerify(bool set_version_in_new_database,
                            DatabaseError&,
                            String& error_message);
  void RunCreationCallback();

  void ScheduleTransaction();

  bool GetVersionFromDatabase(String& version,
                              bool should_cache_version = true);
  bool SetVersionInDatabase(const String& version,
                            bool should_cache_version = true);
  void SetExpectedVersion(const String&);
  const String& ExpectedVersion() const { return expected_version_; }
  String GetCachedVersion() const;
  void SetCachedVersion(const String&);
  bool GetActualVersionForTransaction(String& version);

  void RunTransaction(SQLTransactionCallback*,
                      SQLTransactionErrorCallback*,
                      VoidCallback* success_callback,
                      bool read_only,
                      const ChangeVersionData* = nullptr);
  Vector<String> PerformGetTableNames();

  void ReportOpenDatabaseResult(int error_site,
                                int web_sql_error_code,
                                int sqlite_error_code,
                                double duration);
  void ReportChangeVersionResult(int error_site,
                                 int web_sql_error_code,
                                 int sqlite_error_code);
  void ReportStartTransactionResult(int error_site,
                                    int web_sql_error_code,
                                    int sqlite_error_code);
  void ReportCommitTransactionResult(int error_site,
                                     int web_sql_error_code,
                                     int sqlite_error_code);
  void ReportExecuteStatementResult(int error_site,
                                    int web_sql_error_code,
                                    int sqlite_error_code);
  void ReportVacuumDatabaseResult(int sqlite_error_code);
  void LogErrorMessage(const String&);
  static const char* DatabaseInfoTableName();
  String DatabaseDebugName() const {
    return context_thread_security_origin_->ToString() + "::" + name_;
  }

  RefPtr<SecurityOrigin> context_thread_security_origin_;
  RefPtr<SecurityOrigin> database_thread_security_origin_;
  Member<DatabaseContext>
      database_context_;  // Associated with m_executionContext.
  // TaskRunnerHelper::get is not thread-safe, so we save WebTaskRunner for
  // TaskType::DatabaseAccess for later use as the constructor runs in the main
  // thread.
  RefPtr<WebTaskRunner> database_task_runner_;

  String name_;
  String expected_version_;
  String display_name_;
  unsigned long estimated_size_;
  String filename_;

  DatabaseGuid guid_;
  int opened_;
  bool new_;

  SQLiteDatabase sqlite_database_;

  Member<DatabaseAuthorizer> database_authorizer_;
  TraceWrapperMember<V8DatabaseCallback> creation_callback_;
  Deque<CrossThreadPersistent<SQLTransactionBackend>> transaction_queue_;
  Mutex transaction_in_progress_mutex_;
  bool transaction_in_progress_;
  bool is_transaction_queue_enabled_;

  friend class ChangeVersionWrapper;
  friend class DatabaseManager;
  friend class SQLStatementBackend;
  friend class SQLTransaction;
  friend class SQLTransactionBackend;
};

}  // namespace blink

#endif  // Database_h
