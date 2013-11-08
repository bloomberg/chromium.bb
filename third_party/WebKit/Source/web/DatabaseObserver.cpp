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
#include "modules/webdatabase/DatabaseObserver.h"

#include "WebDatabase.h"
#include "WebDatabaseObserver.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebPermissionClient.h"
#include "WebSecurityOrigin.h"
#include "WebViewImpl.h"
#include "WorkerPermissionClient.h"
#include "bindings/v8/WorkerScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/webdatabase/DatabaseBackendBase.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "platform/CrossThreadCopier.h"

using namespace blink;

namespace WebCore {

// FIXME: Deprecate remaining implementation.
void DatabaseObserver::databaseOpened(DatabaseBackendBase* database)
{
    ASSERT(database->databaseContext()->executionContext()->isContextThread());
    WebDatabase::observer()->databaseOpened(WebDatabase(database));
}

void DatabaseObserver::databaseModified(DatabaseBackendBase* database)
{
    ASSERT(database->databaseContext()->executionContext()->isContextThread());
    WebDatabase::observer()->databaseModified(WebDatabase(database));
}

void DatabaseObserver::databaseClosed(DatabaseBackendBase* database)
{
    ASSERT(database->databaseContext()->executionContext()->isContextThread());
    WebDatabase::observer()->databaseClosed(WebDatabase(database));
}

void DatabaseObserver::reportOpenDatabaseResult(DatabaseBackendBase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportOpenDatabaseResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportChangeVersionResult(DatabaseBackendBase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportChangeVersionResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportStartTransactionResult(DatabaseBackendBase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportStartTransactionResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportCommitTransactionResult(DatabaseBackendBase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportCommitTransactionResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportExecuteStatementResult(DatabaseBackendBase* database, int errorSite, int webSqlErrorCode, int sqliteErrorCode)
{
    WebDatabase::observer()->reportExecuteStatementResult(WebDatabase(database), errorSite, webSqlErrorCode, sqliteErrorCode);
}

void DatabaseObserver::reportVacuumDatabaseResult(DatabaseBackendBase* database, int sqliteErrorCode)
{
    WebDatabase::observer()->reportVacuumDatabaseResult(WebDatabase(database), sqliteErrorCode);
}

} // namespace WebCore
