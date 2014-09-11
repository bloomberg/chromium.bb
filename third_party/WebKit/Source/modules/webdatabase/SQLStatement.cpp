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
#include "config.h"
#include "modules/webdatabase/SQLStatement.h"

#include "platform/Logging.h"
#include "modules/webdatabase/sqlite/SQLiteDatabase.h"
#include "modules/webdatabase/sqlite/SQLiteStatement.h"
#include "modules/webdatabase/AbstractDatabaseServer.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseManager.h"
#include "modules/webdatabase/SQLError.h"
#include "modules/webdatabase/SQLStatementBackend.h"
#include "modules/webdatabase/SQLStatementCallback.h"
#include "modules/webdatabase/SQLStatementErrorCallback.h"
#include "modules/webdatabase/SQLTransaction.h"
#include "wtf/text/CString.h"

namespace blink {

PassOwnPtrWillBeRawPtr<SQLStatement> SQLStatement::create(Database* database,
    PassOwnPtrWillBeRawPtr<SQLStatementCallback> callback, PassOwnPtrWillBeRawPtr<SQLStatementErrorCallback> errorCallback)
{
    return adoptPtrWillBeNoop(new SQLStatement(database, callback, errorCallback));
}

SQLStatement::SQLStatement(Database* database, PassOwnPtrWillBeRawPtr<SQLStatementCallback> callback,
    PassOwnPtrWillBeRawPtr<SQLStatementErrorCallback> errorCallback)
    : m_statementCallbackWrapper(callback, database->executionContext())
    , m_statementErrorCallbackWrapper(errorCallback, database->executionContext())
{
}

SQLStatement::~SQLStatement()
{
}

void SQLStatement::trace(Visitor* visitor)
{
    visitor->trace(m_backend);
    visitor->trace(m_statementCallbackWrapper);
    visitor->trace(m_statementErrorCallbackWrapper);
}

void SQLStatement::setBackend(SQLStatementBackend* backend)
{
    m_backend = backend;
}

bool SQLStatement::hasCallback()
{
    return m_statementCallbackWrapper.hasCallback();
}

bool SQLStatement::hasErrorCallback()
{
    return m_statementErrorCallbackWrapper.hasCallback();
}

bool SQLStatement::performCallback(SQLTransaction* transaction)
{
    ASSERT(transaction);
    ASSERT(m_backend);

    bool callbackError = false;

    OwnPtrWillBeRawPtr<SQLStatementCallback> callback = m_statementCallbackWrapper.unwrap();
    OwnPtrWillBeRawPtr<SQLStatementErrorCallback> errorCallback = m_statementErrorCallbackWrapper.unwrap();
    SQLErrorData* error = m_backend->sqlError();

    // Call the appropriate statement callback and track if it resulted in an error,
    // because then we need to jump to the transaction error callback.
    if (error) {
        if (errorCallback) {
            RefPtrWillBeRawPtr<SQLError> sqlError = SQLError::create(*error);
            callbackError = errorCallback->handleEvent(transaction, sqlError.get());
        }
    } else if (callback) {
        RefPtrWillBeRawPtr<SQLResultSet> resultSet = m_backend->sqlResultSet();
        callbackError = !callback->handleEvent(transaction, resultSet.get());
    }

    return callbackError;
}

} // namespace blink
