// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/database_connections.h"

#include "base/logging.h"

namespace webkit_database {

DatabaseConnections::DatabaseConnections() {
}

DatabaseConnections::~DatabaseConnections() {
  DCHECK(connections_.empty());
}

bool DatabaseConnections::IsDatabaseOpened(const string16& origin_identifier,
                                           const string16& database_name) {
  OriginConnections::const_iterator origin_it =
      connections_.find(origin_identifier);
  if (origin_it == connections_.end())
    return false;
  const DBConnections& origin_connections = origin_it->second;
  return (origin_connections.find(database_name) != origin_connections.end());
}

bool DatabaseConnections::IsOriginUsed(const string16& origin_identifier) {
  return (connections_.find(origin_identifier) != connections_.end());
}

void DatabaseConnections::AddConnection(const string16& origin_identifier,
                                        const string16& database_name) {
  connections_[origin_identifier][database_name]++;
}

void DatabaseConnections::RemoveConnection(const string16& origin_identifier,
                                           const string16& database_name) {
  RemoveConnectionsHelper(origin_identifier, database_name, 1);
}

void DatabaseConnections::RemoveAllConnections() {
  connections_.clear();
}

void DatabaseConnections::RemoveConnections(
    const DatabaseConnections& connections) {
  for (OriginConnections::const_iterator origin_it =
           connections.connections_.begin();
       origin_it != connections.connections_.end();
       origin_it++) {
    const DBConnections& db_connections = origin_it->second;
    for (DBConnections::const_iterator db_it = db_connections.begin();
         db_it != db_connections.end(); db_it++) {
      RemoveConnectionsHelper(origin_it->first, db_it->first, db_it->second);
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
  int& count = db_connections[database_name];
  DCHECK(count >= num_connections);
  count -= num_connections;
  if (!count) {
    db_connections.erase(database_name);
    if (db_connections.empty())
      connections_.erase(origin_iterator);
  }
}

}  // namespace webkit_database
