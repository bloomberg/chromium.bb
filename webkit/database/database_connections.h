// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DATABASE_DATABASE_CONNECTIONS_H_
#define WEBKIT_DATABASE_DATABASE_CONNECTIONS_H_

#include "base/string16.h"

#include <map>

namespace webkit_database {

class DatabaseConnections {
 public:
  DatabaseConnections();
  ~DatabaseConnections();

  bool IsDatabaseOpened(const string16& origin_identifier,
                        const string16& database_name);
  bool IsOriginUsed(const string16& origin_identifier);
  void AddConnection(const string16& origin_identifier,
                     const string16& database_name);
  void RemoveConnection(const string16& origin_identifier,
                        const string16& database_name);
  void RemoveAllConnections();
  void RemoveConnections(const DatabaseConnections& connections);

 private:
  typedef std::map<string16, int> DBConnections;
  typedef std::map<string16, DBConnections> OriginConnections;
  OriginConnections connections_;

  void RemoveConnectionsHelper(const string16& origin_identifier,
                               const string16& database_name,
                               int num_connections);
};

}  // namespace webkit_database

#endif  // WEBKIT_DATABASE_DATABASE_CONNECTIONS_H_
