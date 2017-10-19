/*
 * Copyright (C) 2007, 2013 Apple Inc. All rights reserved.
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
#ifndef SQLTransactionBackend_h
#define SQLTransactionBackend_h

#include <memory>
#include "modules/webdatabase/DatabaseBasicTypes.h"
#include "modules/webdatabase/SQLStatement.h"
#include "modules/webdatabase/SQLTransactionStateMachine.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/ThreadingPrimitives.h"

namespace blink {

class Database;
class SQLErrorData;
class SQLiteTransaction;
class SQLStatementBackend;
class SQLTransaction;
class SQLTransactionBackend;
class SQLValue;

class SQLTransactionWrapper
    : public GarbageCollectedFinalized<SQLTransactionWrapper> {
 public:
  virtual ~SQLTransactionWrapper() {}
  virtual void Trace(blink::Visitor* visitor) {}
  virtual bool PerformPreflight(SQLTransactionBackend*) = 0;
  virtual bool PerformPostflight(SQLTransactionBackend*) = 0;
  virtual SQLErrorData* SqlError() const = 0;
  virtual void HandleCommitFailedAfterPostflight(SQLTransactionBackend*) = 0;
};

class SQLTransactionBackend final
    : public GarbageCollectedFinalized<SQLTransactionBackend>,
      public SQLTransactionStateMachine<SQLTransactionBackend> {
 public:
  static SQLTransactionBackend* Create(Database*,
                                       SQLTransaction*,
                                       SQLTransactionWrapper*,
                                       bool read_only);

  ~SQLTransactionBackend() override;
  void Trace(blink::Visitor*);

  void LockAcquired();
  void PerformNextStep();

  Database* GetDatabase() { return database_.Get(); }
  bool IsReadOnly() { return read_only_; }
  void NotifyDatabaseThreadIsShuttingDown();

  // APIs called from the frontend published:
  void RequestTransitToState(SQLTransactionState);
  SQLErrorData* TransactionError();
  SQLStatement* CurrentStatement();
  void SetShouldRetryCurrentStatement(bool);
  void ExecuteSQL(SQLStatement*,
                  const String& statement,
                  const Vector<SQLValue>& arguments,
                  int permissions);

 private:
  SQLTransactionBackend(Database*,
                        SQLTransaction*,
                        SQLTransactionWrapper*,
                        bool read_only);

  void DoCleanup();

  void EnqueueStatementBackend(SQLStatementBackend*);

  // State Machine functions:
  StateFunction StateFunctionFor(SQLTransactionState) override;
  void ComputeNextStateAndCleanupIfNeeded();

  // State functions:
  SQLTransactionState AcquireLock();
  SQLTransactionState OpenTransactionAndPreflight();
  SQLTransactionState RunStatements();
  SQLTransactionState PostflightAndCommit();
  SQLTransactionState CleanupAndTerminate();
  SQLTransactionState CleanupAfterTransactionErrorCallback();

  SQLTransactionState UnreachableState();
  SQLTransactionState SendToFrontendState();

  SQLTransactionState NextStateForCurrentStatementError();
  SQLTransactionState NextStateForTransactionError();
  SQLTransactionState RunCurrentStatementAndGetNextState();

  void GetNextStatement();

  CrossThreadPersistent<SQLTransaction> frontend_;
  CrossThreadPersistent<SQLStatementBackend> current_statement_backend_;

  Member<Database> database_;
  Member<SQLTransactionWrapper> wrapper_;
  std::unique_ptr<SQLErrorData> transaction_error_;

  bool has_callback_;
  bool has_success_callback_;
  bool has_error_callback_;
  bool should_retry_current_statement_;
  bool modified_database_;
  bool lock_acquired_;
  bool read_only_;
  bool has_version_mismatch_;

  Mutex statement_mutex_;
  Deque<CrossThreadPersistent<SQLStatementBackend>> statement_queue_;

  std::unique_ptr<SQLiteTransaction> sqlite_transaction_;
};

}  // namespace blink

#endif  // SQLTransactionBackend_h
