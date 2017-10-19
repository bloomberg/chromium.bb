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

#include "modules/webdatabase/SQLStatement.h"

#include "core/probe/CoreProbes.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLStatementBackend.h"
#include "modules/webdatabase/SQLStatementCallback.h"
#include "modules/webdatabase/SQLStatementErrorCallback.h"
#include "modules/webdatabase/SQLTransaction.h"
#include "modules/webdatabase/sqlite/SQLiteDatabase.h"
#include "modules/webdatabase/sqlite/SQLiteStatement.h"
#include "platform/wtf/text/CString.h"

namespace blink {

SQLStatement* SQLStatement::Create(Database* database,
                                   SQLStatementCallback* callback,
                                   SQLStatementErrorCallback* error_callback) {
  return new SQLStatement(database, callback, error_callback);
}

SQLStatement::SQLStatement(Database* database,
                           SQLStatementCallback* callback,
                           SQLStatementErrorCallback* error_callback)
    : statement_callback_(callback), statement_error_callback_(error_callback) {
  DCHECK(IsMainThread());

  if (HasCallback() || HasErrorCallback()) {
    probe::AsyncTaskScheduled(database->GetExecutionContext(), "SQLStatement",
                              this);
  }
}

void SQLStatement::Trace(blink::Visitor* visitor) {
  visitor->Trace(backend_);
  visitor->Trace(statement_callback_);
  visitor->Trace(statement_error_callback_);
}

void SQLStatement::SetBackend(SQLStatementBackend* backend) {
  backend_ = backend;
}

bool SQLStatement::HasCallback() {
  return statement_callback_;
}

bool SQLStatement::HasErrorCallback() {
  return statement_error_callback_;
}

bool SQLStatement::PerformCallback(SQLTransaction* transaction) {
  DCHECK(transaction);
  DCHECK(backend_);

  bool callback_error = false;

  SQLStatementCallback* callback = statement_callback_.Release();
  SQLStatementErrorCallback* error_callback =
      statement_error_callback_.Release();
  SQLErrorData* error = backend_->SqlError();

  probe::AsyncTask async_task(transaction->GetDatabase()->GetExecutionContext(),
                              this);

  // Call the appropriate statement callback and track if it resulted in an
  // error, because then we need to jump to the transaction error callback.
  if (error) {
    if (error_callback)
      callback_error =
          error_callback->handleEvent(transaction, SQLError::Create(*error));
  } else if (callback) {
    callback_error =
        !callback->handleEvent(transaction, backend_->SqlResultSet());
  }

  return callback_error;
}

}  // namespace blink
