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

#ifndef SQLTransaction_h
#define SQLTransaction_h

#include <memory>
#include "bindings/core/v8/Nullable.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "modules/webdatabase/SQLStatement.h"
#include "modules/webdatabase/SQLTransactionStateMachine.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Database;
class ExceptionState;
class SQLErrorData;
class SQLStatementCallback;
class SQLStatementErrorCallback;
class SQLTransactionBackend;
class SQLTransactionCallback;
class SQLTransactionErrorCallback;
class SQLValue;
class ScriptValue;
class VoidCallback;

class SQLTransaction final : public GarbageCollectedFinalized<SQLTransaction>,
                             public SQLTransactionStateMachine<SQLTransaction>,
                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SQLTransaction* Create(Database*,
                                SQLTransactionCallback*,
                                VoidCallback* success_callback,
                                SQLTransactionErrorCallback*,
                                bool read_only);
  ~SQLTransaction();
  void Trace(blink::Visitor*);

  void PerformPendingCallback();

  void ExecuteSQL(const String& sql_statement,
                  const Vector<SQLValue>& arguments,
                  SQLStatementCallback*,
                  SQLStatementErrorCallback*,
                  ExceptionState&);
  void executeSql(ScriptState*, const String& sql_statement, ExceptionState&);
  void executeSql(ScriptState*,
                  const String& sql_statement,
                  const Nullable<Vector<ScriptValue>>& arguments,
                  SQLStatementCallback*,
                  SQLStatementErrorCallback*,
                  ExceptionState&);

  Database* GetDatabase() { return database_.Get(); }

  SQLTransactionErrorCallback* ReleaseErrorCallback();

  // APIs called from the backend published:
  void RequestTransitToState(SQLTransactionState);
  bool HasCallback() const;
  bool HasSuccessCallback() const;
  bool HasErrorCallback() const;
  void SetBackend(SQLTransactionBackend*);

 private:
  SQLTransaction(Database*,
                 SQLTransactionCallback*,
                 VoidCallback* success_callback,
                 SQLTransactionErrorCallback*,
                 bool read_only);

  void ClearCallbacks();

  // State Machine functions:
  StateFunction StateFunctionFor(SQLTransactionState) override;
  bool ComputeNextStateAndCleanupIfNeeded();

  // State functions:
  SQLTransactionState DeliverTransactionCallback();
  SQLTransactionState DeliverTransactionErrorCallback();
  SQLTransactionState DeliverStatementCallback();
  SQLTransactionState DeliverQuotaIncreaseCallback();
  SQLTransactionState DeliverSuccessCallback();

  SQLTransactionState UnreachableState();
  SQLTransactionState SendToBackendState();

  SQLTransactionState NextStateForTransactionError();

  Member<Database> database_;
  Member<SQLTransactionBackend> backend_;
  Member<SQLTransactionCallback> callback_;
  Member<VoidCallback> success_callback_;
  Member<SQLTransactionErrorCallback> error_callback_;

  bool execute_sql_allowed_;
  std::unique_ptr<SQLErrorData> transaction_error_;

  bool read_only_;
};

}  // namespace blink

#endif  // SQLTransaction_h
