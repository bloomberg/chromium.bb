/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#include "modules/webdatabase/SQLTransaction.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/VoidCallback.h"
#include "core/probe/CoreProbes.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseAuthorizer.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseThread.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLStatementCallback.h"
#include "modules/webdatabase/SQLStatementErrorCallback.h"
#include "modules/webdatabase/SQLTransactionBackend.h"
#include "modules/webdatabase/SQLTransactionCallback.h"
#include "modules/webdatabase/SQLTransactionClient.h"  // FIXME: Should be used in the backend only.
#include "modules/webdatabase/SQLTransactionErrorCallback.h"
#include "modules/webdatabase/StorageLog.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Vector.h"

namespace blink {

SQLTransaction* SQLTransaction::Create(
    Database* db,
    SQLTransactionCallback* callback,
    VoidCallback* success_callback,
    SQLTransactionErrorCallback* error_callback,
    bool read_only) {
  return new SQLTransaction(db, callback, success_callback, error_callback,
                            read_only);
}

SQLTransaction::SQLTransaction(Database* db,
                               SQLTransactionCallback* callback,
                               VoidCallback* success_callback,
                               SQLTransactionErrorCallback* error_callback,
                               bool read_only)
    : database_(db),
      callback_(callback),
      success_callback_(success_callback),
      error_callback_(error_callback),
      execute_sql_allowed_(false),
      read_only_(read_only) {
  DCHECK(IsMainThread());
  DCHECK(database_);
  probe::AsyncTaskScheduled(db->GetExecutionContext(), "SQLTransaction", this);
}

SQLTransaction::~SQLTransaction() {}

void SQLTransaction::Trace(blink::Visitor* visitor) {
  visitor->Trace(database_);
  visitor->Trace(backend_);
  visitor->Trace(callback_);
  visitor->Trace(success_callback_);
  visitor->Trace(error_callback_);
}

bool SQLTransaction::HasCallback() const {
  return callback_;
}

bool SQLTransaction::HasSuccessCallback() const {
  return success_callback_;
}

bool SQLTransaction::HasErrorCallback() const {
  return error_callback_;
}

void SQLTransaction::SetBackend(SQLTransactionBackend* backend) {
  DCHECK(!backend_);
  backend_ = backend;
}

SQLTransaction::StateFunction SQLTransaction::StateFunctionFor(
    SQLTransactionState state) {
  static const StateFunction kStateFunctions[] = {
      &SQLTransaction::UnreachableState,    // 0. illegal
      &SQLTransaction::UnreachableState,    // 1. idle
      &SQLTransaction::UnreachableState,    // 2. acquireLock
      &SQLTransaction::UnreachableState,    // 3. openTransactionAndPreflight
      &SQLTransaction::SendToBackendState,  // 4. runStatements
      &SQLTransaction::UnreachableState,    // 5. postflightAndCommit
      &SQLTransaction::SendToBackendState,  // 6. cleanupAndTerminate
      &SQLTransaction::
          SendToBackendState,  // 7. cleanupAfterTransactionErrorCallback
      &SQLTransaction::DeliverTransactionCallback,       // 8.
      &SQLTransaction::DeliverTransactionErrorCallback,  // 9.
      &SQLTransaction::DeliverStatementCallback,         // 10.
      &SQLTransaction::DeliverQuotaIncreaseCallback,     // 11.
      &SQLTransaction::DeliverSuccessCallback            // 12.
  };

  DCHECK(WTF_ARRAY_LENGTH(kStateFunctions) ==
         static_cast<int>(SQLTransactionState::kNumberOfStates));
  DCHECK(state < SQLTransactionState::kNumberOfStates);

  return kStateFunctions[static_cast<int>(state)];
}

// requestTransitToState() can be called from the backend. Hence, it should
// NOT be modifying SQLTransactionBackend in general. The only safe field to
// modify is m_requestedState which is meant for this purpose.
void SQLTransaction::RequestTransitToState(SQLTransactionState next_state) {
#if DCHECK_IS_ON()
  STORAGE_DVLOG(1) << "Scheduling " << NameForSQLTransactionState(next_state)
                   << " for transaction " << this;
#endif
  requested_state_ = next_state;
  database_->ScheduleTransactionCallback(this);
}

SQLTransactionState SQLTransaction::NextStateForTransactionError() {
  DCHECK(transaction_error_);
  if (HasErrorCallback())
    return SQLTransactionState::kDeliverTransactionErrorCallback;

  // No error callback, so fast-forward to:
  // Transaction Step 11 - Rollback the transaction.
  return SQLTransactionState::kCleanupAfterTransactionErrorCallback;
}

SQLTransactionState SQLTransaction::DeliverTransactionCallback() {
  bool should_deliver_error_callback = false;
  probe::AsyncTask async_task(database_->GetExecutionContext(), this,
                              "transaction");

  // Spec 4.3.2 4: Invoke the transaction callback with the new SQLTransaction
  // object.
  if (SQLTransactionCallback* callback = callback_.Release()) {
    execute_sql_allowed_ = true;
    should_deliver_error_callback = !callback->handleEvent(this);
    execute_sql_allowed_ = false;
  }

  // Spec 4.3.2 5: If the transaction callback was null or raised an exception,
  // jump to the error callback.
  SQLTransactionState next_state = SQLTransactionState::kRunStatements;
  if (should_deliver_error_callback) {
    database_->ReportStartTransactionResult(5, SQLError::kUnknownErr, 0);
    transaction_error_ = SQLErrorData::Create(
        SQLError::kUnknownErr,
        "the SQLTransactionCallback was null or threw an exception");
    next_state = SQLTransactionState::kDeliverTransactionErrorCallback;
  }
  database_->ReportStartTransactionResult(0, -1, 0);  // OK
  return next_state;
}

SQLTransactionState SQLTransaction::DeliverTransactionErrorCallback() {
  probe::AsyncTask async_task(database_->GetExecutionContext(), this);

  // Spec 4.3.2.10: If exists, invoke error callback with the last
  // error to have occurred in this transaction.
  if (SQLTransactionErrorCallback* error_callback = error_callback_.Release()) {
    // If we get here with an empty m_transactionError, then the backend
    // must be waiting in the idle state waiting for this state to finish.
    // Hence, it's thread safe to fetch the backend transactionError without
    // a lock.
    if (!transaction_error_) {
      DCHECK(backend_->TransactionError());
      transaction_error_ = SQLErrorData::Create(*backend_->TransactionError());
    }
    DCHECK(transaction_error_);
    error_callback->handleEvent(SQLError::Create(*transaction_error_));

    transaction_error_ = nullptr;
  }

  ClearCallbacks();

  // Spec 4.3.2.10: Rollback the transaction.
  return SQLTransactionState::kCleanupAfterTransactionErrorCallback;
}

SQLTransactionState SQLTransaction::DeliverStatementCallback() {
  DCHECK(IsMainThread());
  // Spec 4.3.2.6.6 and 4.3.2.6.3: If the statement callback went wrong, jump to
  // the transaction error callback.  Otherwise, continue to loop through the
  // statement queue.
  execute_sql_allowed_ = true;

  SQLStatement* current_statement = backend_->CurrentStatement();
  DCHECK(current_statement);

  bool result = current_statement->PerformCallback(this);

  execute_sql_allowed_ = false;

  if (result) {
    database_->ReportCommitTransactionResult(2, SQLError::kUnknownErr, 0);
    transaction_error_ =
        SQLErrorData::Create(SQLError::kUnknownErr,
                             "the statement callback raised an exception or "
                             "statement error callback did not return false");
    return NextStateForTransactionError();
  }
  return SQLTransactionState::kRunStatements;
}

SQLTransactionState SQLTransaction::DeliverQuotaIncreaseCallback() {
  DCHECK(IsMainThread());
  DCHECK(backend_->CurrentStatement());

  bool should_retry_current_statement =
      database_->TransactionClient()->DidExceedQuota(GetDatabase());
  backend_->SetShouldRetryCurrentStatement(should_retry_current_statement);

  return SQLTransactionState::kRunStatements;
}

SQLTransactionState SQLTransaction::DeliverSuccessCallback() {
  DCHECK(IsMainThread());
  probe::AsyncTask async_task(database_->GetExecutionContext(), this);

  // Spec 4.3.2.8: Deliver success callback.
  if (VoidCallback* success_callback = success_callback_.Release())
    success_callback->handleEvent();

  ClearCallbacks();

  // Schedule a "post-success callback" step to return control to the database
  // thread in case there are further transactions queued up for this Database.
  return SQLTransactionState::kCleanupAndTerminate;
}

// This state function is used as a stub function to plug unimplemented states
// in the state dispatch table. They are unimplemented because they should
// never be reached in the course of correct execution.
SQLTransactionState SQLTransaction::UnreachableState() {
  NOTREACHED();
  return SQLTransactionState::kEnd;
}

SQLTransactionState SQLTransaction::SendToBackendState() {
  DCHECK_NE(next_state_, SQLTransactionState::kIdle);
  backend_->RequestTransitToState(next_state_);
  return SQLTransactionState::kIdle;
}

void SQLTransaction::PerformPendingCallback() {
  DCHECK(IsMainThread());
  ComputeNextStateAndCleanupIfNeeded();
  RunStateMachine();
}

void SQLTransaction::ExecuteSQL(const String& sql_statement,
                                const Vector<SQLValue>& arguments,
                                SQLStatementCallback* callback,
                                SQLStatementErrorCallback* callback_error,
                                ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  if (!execute_sql_allowed_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "SQL execution is disallowed.");
    return;
  }

  if (!database_->Opened()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The database has not been opened.");
    return;
  }

  int permissions = DatabaseAuthorizer::kReadWriteMask;
  if (!database_->GetDatabaseContext()->AllowDatabaseAccess())
    permissions |= DatabaseAuthorizer::kNoAccessMask;
  else if (read_only_)
    permissions |= DatabaseAuthorizer::kReadOnlyMask;

  SQLStatement* statement =
      SQLStatement::Create(database_.Get(), callback, callback_error);
  backend_->ExecuteSQL(statement, sql_statement, arguments, permissions);
}

void SQLTransaction::executeSql(ScriptState* script_state,
                                const String& sql_statement,
                                ExceptionState& exception_state) {
  ExecuteSQL(sql_statement, Vector<SQLValue>(), nullptr, nullptr,
             exception_state);
}

void SQLTransaction::executeSql(ScriptState* script_state,
                                const String& sql_statement,
                                const Nullable<Vector<ScriptValue>>& arguments,
                                SQLStatementCallback* callback,
                                SQLStatementErrorCallback* callback_error,
                                ExceptionState& exception_state) {
  Vector<SQLValue> sql_values;
  if (!arguments.IsNull())
    sql_values = ToImplArray<Vector<SQLValue>>(
        arguments.Get(), script_state->GetIsolate(), exception_state);
  ExecuteSQL(sql_statement, sql_values, callback, callback_error,
             exception_state);
}

bool SQLTransaction::ComputeNextStateAndCleanupIfNeeded() {
  // Only honor the requested state transition if we're not supposed to be
  // cleaning up and shutting down:
  if (database_->Opened()) {
    SetStateToRequestedState();
    DCHECK(next_state_ == SQLTransactionState::kEnd ||
           next_state_ == SQLTransactionState::kDeliverTransactionCallback ||
           next_state_ ==
               SQLTransactionState::kDeliverTransactionErrorCallback ||
           next_state_ == SQLTransactionState::kDeliverStatementCallback ||
           next_state_ == SQLTransactionState::kDeliverQuotaIncreaseCallback ||
           next_state_ == SQLTransactionState::kDeliverSuccessCallback);
#if DCHECK_IS_ON()
    STORAGE_DVLOG(1) << "Callback " << NameForSQLTransactionState(next_state_);
#endif
    return false;
  }

  ClearCallbacks();
  next_state_ = SQLTransactionState::kCleanupAndTerminate;

  return true;
}

void SQLTransaction::ClearCallbacks() {
  callback_.Clear();
  success_callback_.Clear();
  error_callback_.Clear();
}

SQLTransactionErrorCallback* SQLTransaction::ReleaseErrorCallback() {
  return error_callback_.Release();
}

}  // namespace blink
