// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/databases_table.h"

#include "app/sql/connection.h"
#include "app/sql/statement.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace webkit_database {

bool DatabasesTable::Init() {
  // 'Databases' schema:
  //   id              A unique ID assigned to each database
  //   origin          The originto which the database belongs. This is a
  //                   string that can be used as part of a file name
  //                   (http_webkit.org_0, for example).
  //   name            The database name.
  //   description     A short description of the database.
  //   estimated_size  The estimated size of the database.
  return db_->DoesTableExist("Databases") ||
      (db_->Execute(
           "CREATE TABLE Databases ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT, "
           "origin TEXT NOT NULL, "
           "name TEXT NOT NULL, "
           "description TEXT NOT NULL, "
           "estimated_size INTEGER NOT NULL)") &&
       db_->Execute(
           "CREATE INDEX origin_index ON Databases (origin)") &&
       db_->Execute(
           "CREATE UNIQUE INDEX unique_index ON Databases (origin, name)"));
}

int64 DatabasesTable::GetDatabaseID(const string16& origin_identifier,
                                    const string16& database_name) {
  sql::Statement select_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT id FROM Databases WHERE origin = ? AND name = ?"));
  if (select_statement.is_valid() &&
      select_statement.BindString(0, UTF16ToUTF8(origin_identifier)) &&
      select_statement.BindString(1, UTF16ToUTF8(database_name)) &&
      select_statement.Step()) {
    return select_statement.ColumnInt64(0);
  }

  return -1;
}

bool DatabasesTable::GetDatabaseDetails(const string16& origin_identifier,
                                        const string16& database_name,
                                        DatabaseDetails* details) {
  DCHECK(details);
  sql::Statement select_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT description, estimated_size FROM Databases "
                     "WHERE origin = ? AND name = ?"));
  if (select_statement.is_valid() &&
      select_statement.BindString(0, UTF16ToUTF8(origin_identifier)) &&
      select_statement.BindString(1, UTF16ToUTF8(database_name)) &&
      select_statement.Step()) {
    details->origin_identifier = origin_identifier;
    details->database_name = database_name;
    details->description = UTF8ToUTF16(select_statement.ColumnString(0));
    details->estimated_size = select_statement.ColumnInt64(1);
    return true;
  }

  return false;
}

bool DatabasesTable::InsertDatabaseDetails(const DatabaseDetails& details) {
  sql::Statement insert_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "INSERT INTO Databases (origin, name, description, "
                     "estimated_size) VALUES (?, ?, ?, ?)"));
  if (insert_statement.is_valid() &&
      insert_statement.BindString(0, UTF16ToUTF8(details.origin_identifier)) &&
      insert_statement.BindString(1, UTF16ToUTF8(details.database_name)) &&
      insert_statement.BindString(2, UTF16ToUTF8(details.description)) &&
      insert_statement.BindInt64(3, details.estimated_size)) {
    return insert_statement.Run();
  }

  return false;
}

bool DatabasesTable::UpdateDatabaseDetails(const DatabaseDetails& details) {
  sql::Statement update_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "UPDATE Databases SET description = ?, "
                     "estimated_size = ? WHERE origin = ? AND name = ?"));
  if (update_statement.is_valid() &&
      update_statement.BindString(0, UTF16ToUTF8(details.description)) &&
      update_statement.BindInt64(1, details.estimated_size) &&
      update_statement.BindString(2, UTF16ToUTF8(details.origin_identifier)) &&
      update_statement.BindString(3, UTF16ToUTF8(details.database_name))) {
    return (update_statement.Run() && db_->GetLastChangeCount());
  }

  return false;
}

bool DatabasesTable::DeleteDatabaseDetails(const string16& origin_identifier,
                                           const string16& database_name) {
  sql::Statement delete_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM Databases WHERE origin = ? AND name = ?"));
  if (delete_statement.is_valid() &&
      delete_statement.BindString(0, UTF16ToUTF8(origin_identifier)) &&
      delete_statement.BindString(1, UTF16ToUTF8(database_name))) {
    return (delete_statement.Run() && db_->GetLastChangeCount());
  }

  return false;
}

bool DatabasesTable::GetAllOrigins(std::vector<string16>* origins) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT DISTINCT origin FROM Databases ORDER BY origin"));
  if (statement.is_valid()) {
    while (statement.Step())
      origins->push_back(UTF8ToUTF16(statement.ColumnString(0)));
    return statement.Succeeded();
  }

  return false;
}

bool DatabasesTable::GetAllDatabaseDetailsForOrigin(
    const string16& origin_identifier,
    std::vector<DatabaseDetails>* details_vector) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT name, description, estimated_size "
                     "FROM Databases WHERE origin = ? ORDER BY name"));
  if (statement.is_valid() &&
      statement.BindString(0, UTF16ToUTF8(origin_identifier))) {
    while (statement.Step()) {
      DatabaseDetails details;
      details.origin_identifier = origin_identifier;
      details.database_name = UTF8ToUTF16(statement.ColumnString(0));
      details.description = UTF8ToUTF16(statement.ColumnString(1));
      details.estimated_size = statement.ColumnInt64(2);
      details_vector->push_back(details);
    }
    return statement.Succeeded();
  }

  return false;
}

bool DatabasesTable::DeleteOrigin(const string16& origin_identifier) {
  sql::Statement delete_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM Databases WHERE origin = ?"));
  if (delete_statement.is_valid() &&
      delete_statement.BindString(0, UTF16ToUTF8(origin_identifier))) {
    return (delete_statement.Run() && db_->GetLastChangeCount());
  }

  return false;
}

}  // namespace webkit_database
