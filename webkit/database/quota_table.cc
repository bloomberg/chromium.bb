// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/quota_table.h"

#include "app/sql/connection.h"
#include "app/sql/statement.h"
#include "base/string_util.h"

namespace webkit_database {

bool QuotaTable::Init() {
  // 'Quota' schema:
  //   origin          The origin.
  //   quota           The quota for this origin.
  return db_->DoesTableExist("Quota") ||
      db_->Execute(
           "CREATE TABLE Quota ("
           "origin TEXT NOT NULL PRIMARY KEY, "
           "quota INTEGER NOT NULL)");
}

int64 QuotaTable::GetOriginQuota(const string16& origin_identifier) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT quota FROM Quota WHERE origin = ?"));
  if (statement.is_valid() &&
      statement.BindString(0, UTF16ToUTF8(origin_identifier)) &&
      statement.Step()) {
    return statement.ColumnInt64(0);
  }

  return -1;
}

bool QuotaTable::SetOriginQuota(const string16& origin_identifier,
                                int64 quota) {
  DCHECK(quota >= 0);

  // Insert or update the quota for this origin.
  sql::Statement replace_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "REPLACE INTO Quota VALUES (?, ?)"));
  if (replace_statement.is_valid() &&
      replace_statement.BindString(0, UTF16ToUTF8(origin_identifier)) &&
      replace_statement.BindInt64(1, quota)) {
    return replace_statement.Run();
  }

  return false;
}

bool QuotaTable::ClearOriginQuota(const string16& origin_identifier) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM Quota WHERE origin = ?"));
  if (statement.is_valid() &&
      statement.BindString(0, UTF16ToUTF8(origin_identifier))) {
    return (statement.Run() && db_->GetLastChangeCount());
  }

  return false;
}

}  // namespace webkit_database
