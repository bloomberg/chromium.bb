// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_connections.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"

namespace webkit_database {

DatabaseConnections::DatabaseConnections() {
}

DatabaseConnections::~DatabaseConnections() {
  DCHECK(connections_.empty());
}

bool DatabaseConnections::IsEmpty() const {
  return connections_.empty();
}

bool DatabaseConnections::IsDatabaseOpened(
    const string16& origin_identifier,
    const string16& database_name) const {
  OriginConnections::const_iterator origin_it =
      connections_.find(origin_identifier);
  if (origin_it == connections_.end())
    return false;
  const DBConnections& origin_connections = origin_it->second;
  return (origin_connections.find(database_name) != origin_connections.end());
}

bool DatabaseConnections::IsOriginUsed(
    const string16& origin_identifier) const {
  return (connections_.find(origin_identifier) != connections_.end());
}

void DatabaseConnections::AddConnection(const string16& origin_identifier,
                                        const string16& database_name) {
  connections_[origin_identifier][database_name].first++;
}

void DatabaseConnections::RemoveConnection(const string16& origin_identifier,
                                           const string16& database_name) {
  RemoveConnectionsHelper(origin_identifier, database_name, 1);
}

void DatabaseConnections::RemoveAllConnections() {
  connections_.clear();
}

void DatabaseConnections::RemoveConnections(
    const DatabaseConnections& connections,
    std::vector<std::pair<string16, string16> >* closed_dbs) {
  for (OriginConnections::const_iterator origin_it =
           connections.connections_.begin();
       origin_it != connections.connections_.end();
       origin_it++) {
    const DBConnections& db_connections = origin_it->second;
    for (DBConnections::const_iterator db_it = db_connections.begin();
         db_it != db_connections.end(); db_it++) {
      RemoveConnectionsHelper(origin_it->first, db_it->first,
                              db_it->second.first);
      if (!IsDatabaseOpened(origin_it->first, db_it->first))
        closed_dbs->push_back(std::make_pair(origin_it->first, db_it->first));
    }
  }
}

int64 DatabaseConnections::GetOpenDatabaseSize(
    const string16& origin_identifier,
    const string16& database_name) const {
  DCHECK(IsDatabaseOpened(origin_identifier, database_name));
  return connections_[origin_identifier][database_name].second;
}

void DatabaseConnections::SetOpenDatabaseSize(
    const string16& origin_identifier,
    const string16& database_name,
    int64 size) {
  DCHECK(IsDatabaseOpened(origin_identifier, database_name));
  connections_[origin_identifier][database_name].second = size;
}

void DatabaseConnections::ListConnections(
    std::vector<std::pair<string16, string16> > *list) const {
  for (OriginConnections::const_iterator origin_it =
           connections_.begin();
       origin_it != connections_.end();
       origin_it++) {
    const DBConnections& db_connections = origin_it->second;
    for (DBConnections::const_iterator db_it = db_connections.begin();
         db_it != db_connections.end(); db_it++) {
      list->push_back(std::make_pair(origin_it->first, db_it->first));
    }
  }
}

void DatabaseConnections::RemoveConnectionsHelper(
    const string16& origin_identifier,
    const string16& database_name,
    int num_connections) {
  OriginConnections::iterator origin_iterator =
      connections_.find(origin_identifier);
  DCHECK(origin_iterator != connections_.end());
  DBConnections& db_connections = origin_iterator->second;
  int& count = db_connections[database_name].first;
  DCHECK(count >= num_connections);
  count -= num_connections;
  if (!count) {
    db_connections.erase(database_name);
    if (db_connections.empty())
      connections_.erase(origin_iterator);
  }
}

DatabaseConnectionsWrapper::DatabaseConnectionsWrapper()
    : waiting_for_dbs_to_close_(false),
      main_thread_(base::MessageLoopProxy::CreateForCurrentThread()) {
}

DatabaseConnectionsWrapper::~DatabaseConnectionsWrapper() {
}

void DatabaseConnectionsWrapper::WaitForAllDatabasesToClose() {
  // We assume that new databases won't be open while we're waiting.
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (HasOpenConnections()) {
    AutoReset<bool> auto_reset(&waiting_for_dbs_to_close_, true);
    MessageLoop::ScopedNestableTaskAllower nestable(MessageLoop::current());
    MessageLoop::current()->Run();
  }
}

bool DatabaseConnectionsWrapper::HasOpenConnections() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  base::AutoLock auto_lock(open_connections_lock_);
  return !open_connections_.IsEmpty();
}

void DatabaseConnectionsWrapper::AddOpenConnection(
    const string16& origin_identifier,
    const string16& database_name) {
  // We add to the collection immediately on any thread.
  base::AutoLock auto_lock(open_connections_lock_);
  open_connections_.AddConnection(origin_identifier, database_name);
}

void DatabaseConnectionsWrapper::RemoveOpenConnection(
    const string16& origin_identifier,
    const string16& database_name) {
  // But only remove from the collection on the main thread
  // so we can handle the waiting_for_dbs_to_close_ case.
  if (!main_thread_->BelongsToCurrentThread()) {
    main_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &DatabaseConnectionsWrapper::RemoveOpenConnection,
        origin_identifier, database_name));
    return;
  }
  base::AutoLock auto_lock(open_connections_lock_);
  open_connections_.RemoveConnection(origin_identifier, database_name);
  if (waiting_for_dbs_to_close_ && open_connections_.IsEmpty())
    MessageLoop::current()->Quit();
}

}  // namespace webkit_database
