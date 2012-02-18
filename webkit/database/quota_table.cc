// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/quota_table.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "sql/statement.h"

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
  statement.BindString16(0, origin_identifier);
  if (statement.Step()) {
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
  replace_statement.BindString16(0, origin_identifier);
  replace_statement.BindInt64(1, quota);
  return replace_statement.Run();
}

bool QuotaTable::ClearOriginQuota(const string16& origin_identifier) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM Quota WHERE origin = ?"));
  statement.BindString16(0, origin_identifier);

  return (statement.Run() && db_->GetLastChangeCount());
}

}  // namespace webkit_database
