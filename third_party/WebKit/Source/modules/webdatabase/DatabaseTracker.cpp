/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "modules/webdatabase/DatabaseTracker.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseClient.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/QuotaTracker.h"
#include "modules/webdatabase/sqlite/SQLiteFileSystem.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityOriginHash.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/StdLibExtras.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDatabaseObserver.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

static void DatabaseClosed(Database* database) {
  if (Platform::Current()->DatabaseObserver()) {
    Platform::Current()->DatabaseObserver()->DatabaseClosed(
        WebSecurityOrigin(database->GetSecurityOrigin()),
        database->StringIdentifier());
  }
}

DatabaseTracker& DatabaseTracker::Tracker() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(DatabaseTracker, tracker, ());
  return tracker;
}

DatabaseTracker::DatabaseTracker() {
  SQLiteFileSystem::RegisterSQLiteVFS();
}

bool DatabaseTracker::CanEstablishDatabase(DatabaseContext* database_context,
                                           const String& name,
                                           const String& display_name,
                                           unsigned estimated_size,
                                           DatabaseError& error) {
  ExecutionContext* execution_context = database_context->GetExecutionContext();
  bool success = DatabaseClient::From(execution_context)
                     ->AllowDatabase(execution_context, name, display_name,
                                     estimated_size);
  if (!success)
    error = DatabaseError::kGenericSecurityError;
  return success;
}

String DatabaseTracker::FullPathForDatabase(SecurityOrigin* origin,
                                            const String& name,
                                            bool) {
  return String(Platform::Current()->DatabaseCreateOriginIdentifier(
             WebSecurityOrigin(origin))) +
         "/" + name + "#";
}

void DatabaseTracker::AddOpenDatabase(Database* database) {
  MutexLocker open_database_map_lock(open_database_map_guard_);
  if (!open_database_map_)
    open_database_map_ = WTF::WrapUnique(new DatabaseOriginMap);

  String origin_string = database->GetSecurityOrigin()->ToRawString();
  DatabaseNameMap* name_map = open_database_map_->at(origin_string);
  if (!name_map) {
    name_map = new DatabaseNameMap();
    open_database_map_->Set(origin_string, name_map);
  }

  String name(database->StringIdentifier());
  DatabaseSet* database_set = name_map->at(name);
  if (!database_set) {
    database_set = new DatabaseSet();
    name_map->Set(name, database_set);
  }

  database_set->insert(database);
}

void DatabaseTracker::RemoveOpenDatabase(Database* database) {
  {
    MutexLocker open_database_map_lock(open_database_map_guard_);
    String origin_string = database->GetSecurityOrigin()->ToRawString();
    DCHECK(open_database_map_);
    DatabaseNameMap* name_map = open_database_map_->at(origin_string);
    if (!name_map)
      return;

    String name(database->StringIdentifier());
    DatabaseSet* database_set = name_map->at(name);
    if (!database_set)
      return;

    DatabaseSet::iterator found = database_set->find(database);
    if (found == database_set->end())
      return;

    database_set->erase(found);
    if (database_set->IsEmpty()) {
      name_map->erase(name);
      delete database_set;
      if (name_map->IsEmpty()) {
        open_database_map_->erase(origin_string);
        delete name_map;
      }
    }
  }
  DatabaseClosed(database);
}

void DatabaseTracker::PrepareToOpenDatabase(Database* database) {
  DCHECK(
      database->GetDatabaseContext()->GetExecutionContext()->IsContextThread());
  if (Platform::Current()->DatabaseObserver()) {
    Platform::Current()->DatabaseObserver()->DatabaseOpened(
        WebSecurityOrigin(database->GetSecurityOrigin()),
        database->StringIdentifier(), database->DisplayName(),
        database->EstimatedSize());
  }
}

void DatabaseTracker::FailedToOpenDatabase(Database* database) {
  DatabaseClosed(database);
}

unsigned long long DatabaseTracker::GetMaxSizeForDatabase(
    const Database* database) {
  unsigned long long space_available = 0;
  unsigned long long database_size = 0;
  QuotaTracker::Instance().GetDatabaseSizeAndSpaceAvailableToOrigin(
      database->GetSecurityOrigin(), database->StringIdentifier(),
      &database_size, &space_available);
  return database_size + space_available;
}

void DatabaseTracker::CloseDatabasesImmediately(SecurityOrigin* origin,
                                                const String& name) {
  String origin_string = origin->ToRawString();
  MutexLocker open_database_map_lock(open_database_map_guard_);
  if (!open_database_map_)
    return;

  DatabaseNameMap* name_map = open_database_map_->at(origin_string);
  if (!name_map)
    return;

  DatabaseSet* database_set = name_map->at(name);
  if (!database_set)
    return;

  // We have to call closeImmediately() on the context thread.
  for (DatabaseSet::iterator it = database_set->begin();
       it != database_set->end(); ++it) {
    (*it)->GetDatabaseTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&DatabaseTracker::CloseOneDatabaseImmediately,
                        CrossThreadUnretained(this), origin_string, name, *it));
  }
}

void DatabaseTracker::ForEachOpenDatabaseInPage(
    Page* page,
    std::unique_ptr<DatabaseCallback> callback) {
  MutexLocker open_database_map_lock(open_database_map_guard_);
  if (!open_database_map_)
    return;
  for (auto& origin_map : *open_database_map_) {
    for (auto& name_database_set : *origin_map.value) {
      for (Database* database : *name_database_set.value) {
        ExecutionContext* context = database->GetExecutionContext();
        DCHECK(context->IsDocument());
        if (ToDocument(context)->GetFrame()->GetPage() == page)
          (*callback)(database);
      }
    }
  }
}

void DatabaseTracker::CloseOneDatabaseImmediately(const String& origin_string,
                                                  const String& name,
                                                  Database* database) {
  // First we have to confirm the 'database' is still in our collection.
  {
    MutexLocker open_database_map_lock(open_database_map_guard_);
    if (!open_database_map_)
      return;

    DatabaseNameMap* name_map = open_database_map_->at(origin_string);
    if (!name_map)
      return;

    DatabaseSet* database_set = name_map->at(name);
    if (!database_set)
      return;

    DatabaseSet::iterator found = database_set->find(database);
    if (found == database_set->end())
      return;
  }

  // And we have to call closeImmediately() without our collection lock being
  // held.
  database->CloseImmediately();
}

}  // namespace blink
