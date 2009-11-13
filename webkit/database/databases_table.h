// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DATABASE_DATABASES_TABLE_H_
#define WEBKIT_DATABASE_DATABASES_TABLE_H_

#include <vector>

#include "base/string16.h"

namespace sql {
class Connection;
}

namespace webkit_database {

struct DatabaseDetails {
  DatabaseDetails() : estimated_size(0) { }

  string16 origin_identifier;
  string16 database_name;
  string16 description;
  int64 estimated_size;
};

class DatabasesTable {
 public:
  explicit DatabasesTable(sql::Connection* db) : db_(db) { }

  bool Init();
  int64 GetDatabaseID(const string16& origin_identifier,
                      const string16& database_name);
  bool GetDatabaseDetails(const string16& origin_identifier,
                          const string16& database_name,
                          DatabaseDetails* details);
  bool InsertDatabaseDetails(const DatabaseDetails& details);
  bool UpdateDatabaseDetails(const DatabaseDetails& details);
  bool DeleteDatabaseDetails(const string16& origin_identifier,
                             const string16& database_name);
  bool GetAllDatabaseDetailsForOrigin(const string16& origin_identifier,
                                      std::vector<DatabaseDetails>* details);
 private:
  sql::Connection* db_;
};

}  // namespace webkit_database

#endif  // WEBKIT_DATABASE_DATABASES_TABLE_H_
