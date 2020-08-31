// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/compromised_credentials_table.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/password_manager/core/browser/sql_table_builder.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/core/features.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace password_manager {
namespace {

constexpr char kCompromisedCredentialsTableName[] = "compromised_credentials";

// Represents columns of the compromised credentials table. Used with SQL
// queries that use all the columns.
enum class CompromisedCredentialsTableColumn {
  kSignonRealm,
  kUsername,
  kCreateTime,
  kCompromiseType,
  kMaxValue = kCompromiseType
};

// Casts the compromised credentials table column enum to its integer value.
int GetColumnNumber(CompromisedCredentialsTableColumn column) {
  return static_cast<int>(column);
}

// Teaches |builder| about the different DB schemes in different versions.
void InitializeCompromisedCredentialsBuilder(SQLTableBuilder* builder) {
  // Version 0.
  builder->AddColumnToUniqueKey("url", "VARCHAR NOT NULL");
  builder->AddColumnToUniqueKey("username", "VARCHAR NOT NULL");
  builder->AddColumn("create_time", "INTEGER NOT NULL");
  builder->AddColumnToUniqueKey("compromise_type", "INTEGER NOT NULL");
  builder->AddIndex("compromised_credentials_index",
                    {"url", "username", "compromise_type"});
  builder->SealVersion();
}

// Returns a compromised credentials vector from the SQL statement.
std::vector<CompromisedCredentials> StatementToCompromisedCredentials(
    sql::Statement* s) {
  std::vector<CompromisedCredentials> results;
  while (s->Step()) {
    std::string signon_realm(s->ColumnString(
        GetColumnNumber(CompromisedCredentialsTableColumn::kSignonRealm)));
    base::string16 username = s->ColumnString16(
        GetColumnNumber(CompromisedCredentialsTableColumn::kUsername));
    base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
        (base::TimeDelta::FromMicroseconds(s->ColumnInt64(
            GetColumnNumber(CompromisedCredentialsTableColumn::kCreateTime)))));
    CompromiseType compromise_type = static_cast<CompromiseType>(s->ColumnInt64(
        GetColumnNumber(CompromisedCredentialsTableColumn::kCompromiseType)));
    results.push_back({std::move(signon_realm), std::move(username),
                       create_time, compromise_type});
  }
  return results;
}

}  // namespace

bool operator==(const CompromisedCredentials& lhs,
                const CompromisedCredentials& rhs) {
  return lhs.signon_realm == rhs.signon_realm && lhs.username == rhs.username &&
         lhs.create_time == rhs.create_time &&
         lhs.compromise_type == rhs.compromise_type;
}

void CompromisedCredentialsTable::Init(sql::Database* db) {
  bool password_protection_show_domains_for_saved_password_is_on =
      base::FeatureList::IsEnabled(
          safe_browsing::kPasswordProtectionShowDomainsForSavedPasswords);
  if (password_protection_show_domains_for_saved_password_is_on ||
      base::FeatureList::IsEnabled(password_manager::features::kPasswordCheck))
    db_ = db;
}

bool CompromisedCredentialsTable::CreateTableIfNecessary() {
  if (!db_)
    return false;

  if (db_->DoesTableExist(kCompromisedCredentialsTableName))
    return true;

  SQLTableBuilder builder(kCompromisedCredentialsTableName);
  InitializeCompromisedCredentialsBuilder(&builder);
  return builder.CreateTable(db_);
}

bool CompromisedCredentialsTable::AddRow(
    const CompromisedCredentials& compromised_credentials) {
  if (!db_ || compromised_credentials.signon_realm.empty())
    return false;

  DCHECK(db_->DoesTableExist(kCompromisedCredentialsTableName));

  base::UmaHistogramEnumeration("PasswordManager.CompromisedCredentials.Add",
                                compromised_credentials.compromise_type);

  // In case there is an error, expect it to be a constraint violation.
  db_->set_error_callback(base::BindRepeating([](int error, sql::Statement*) {
    constexpr int kSqliteErrorMask = 0xFF;
    constexpr int kSqliteConstraint = 19;
    if ((error & kSqliteErrorMask) != kSqliteConstraint) {
      DLOG(ERROR) << "Got unexpected SQL error code: " << error;
    }
  }));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO compromised_credentials (url, username, create_time, "
      "compromise_type) VALUES (?, ?, ?, ?)"));

  s.BindString(GetColumnNumber(CompromisedCredentialsTableColumn::kSignonRealm),
               compromised_credentials.signon_realm);
  s.BindString16(GetColumnNumber(CompromisedCredentialsTableColumn::kUsername),
                 compromised_credentials.username);
  s.BindInt64(GetColumnNumber(CompromisedCredentialsTableColumn::kCreateTime),
              compromised_credentials.create_time.ToDeltaSinceWindowsEpoch()
                  .InMicroseconds());
  s.BindInt64(
      GetColumnNumber(CompromisedCredentialsTableColumn::kCompromiseType),
      static_cast<int>(compromised_credentials.compromise_type));

  bool result = s.Run();
  db_->reset_error_callback();
  return result;
}

std::vector<CompromisedCredentials> CompromisedCredentialsTable::GetRows(
    const std::string& signon_realm,
    const base::string16& username) const {
  if (!db_ || signon_realm.empty())
    return std::vector<CompromisedCredentials>{};

  DCHECK(db_->DoesTableExist(kCompromisedCredentialsTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT * FROM compromised_credentials WHERE url = ? AND username = ? "));
  s.BindString(0, signon_realm);
  s.BindString16(1, username);
  return StatementToCompromisedCredentials(&s);
}

std::vector<CompromisedCredentials> CompromisedCredentialsTable::GetRows(
    const std::string& signon_realm) const {
  if (!db_ || signon_realm.empty())
    return std::vector<CompromisedCredentials>{};

  DCHECK(db_->DoesTableExist(kCompromisedCredentialsTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE, "SELECT * FROM compromised_credentials WHERE url = ? "));
  s.BindString(0, signon_realm);
  return StatementToCompromisedCredentials(&s);
}

bool CompromisedCredentialsTable::UpdateRow(
    const std::string& new_signon_realm,
    const base::string16& new_username,
    const std::string& old_signon_realm,
    const base::string16& old_username) const {
  if (!db_ || new_signon_realm.empty() || old_signon_realm.empty())
    return false;

  DCHECK(db_->DoesTableExist(kCompromisedCredentialsTableName));

  // Retrieve the rows that are to be updated to log.
  const std::vector<CompromisedCredentials> compromised_credentials =
      GetRows(old_signon_realm, old_username);
  if (compromised_credentials.empty())
    return false;
  for (const auto& compromised_credential : compromised_credentials) {
    base::UmaHistogramEnumeration(
        "PasswordManager.CompromisedCredentials.Update",
        compromised_credential.compromise_type);
  }

  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE,
                                           "UPDATE compromised_credentials "
                                           "SET url = ?, username = ? "
                                           "WHERE url = ? and username = ?"));
  s.BindString(0, new_signon_realm);
  s.BindString16(1, new_username);
  s.BindString(2, old_signon_realm);
  s.BindString16(3, old_username);
  return s.Run();
}

bool CompromisedCredentialsTable::RemoveRow(
    const std::string& signon_realm,
    const base::string16& username,
    RemoveCompromisedCredentialsReason reason) {
  if (!db_ || signon_realm.empty())
    return false;

  DCHECK(db_->DoesTableExist(kCompromisedCredentialsTableName));

  // Retrieve the rows that are to be removed to log.
  const std::vector<CompromisedCredentials> compromised_credentials =
      GetRows(signon_realm, username);
  if (compromised_credentials.empty())
    return false;
  for (const auto& compromised_credential : compromised_credentials) {
    base::UmaHistogramEnumeration(
        "PasswordManager.CompromisedCredentials.Remove",
        compromised_credential.compromise_type);
    base::UmaHistogramEnumeration(
        "PasswordManager.RemoveCompromisedCredentials.RemoveReason", reason);
  }

  sql::Statement s(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "DELETE FROM compromised_credentials WHERE "
                              "url = ? AND username = ? "));
  s.BindString(0, signon_realm);
  s.BindString16(1, username);
  return s.Run();
}

bool CompromisedCredentialsTable::RemoveRowByCompromiseType(
    const std::string& signon_realm,
    const base::string16& username,
    const CompromiseType& compromise_type,
    RemoveCompromisedCredentialsReason reason) {
  if (!db_ || signon_realm.empty())
    return false;

  DCHECK(db_->DoesTableExist(kCompromisedCredentialsTableName));

  // Retrieve the rows that are to be removed to log.
  const std::vector<CompromisedCredentials> compromised_credentials =
      GetRowByCompromiseType(signon_realm, username, compromise_type);
  if (compromised_credentials.empty())
    return false;

  for (const auto& compromised_credential : compromised_credentials) {
    base::UmaHistogramEnumeration(
        "PasswordManager.CompromisedCredentials.Remove",
        compromised_credential.compromise_type);
    base::UmaHistogramEnumeration(
        "PasswordManager.RemoveCompromisedCredentials.RemoveReason", reason);
  }

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM compromised_credentials WHERE "
      "url = ? AND username = ? AND compromise_type = ?"));
  s.BindString(0, signon_realm);
  s.BindString16(1, username);
  s.BindInt64(2, static_cast<int>(compromise_type));
  return s.Run();
}

bool CompromisedCredentialsTable::RemoveRowsByUrlAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  if (!db_)
    return false;

  DCHECK(db_->DoesTableExist(kCompromisedCredentialsTableName));

  const int64_t remove_begin_us =
      remove_begin.ToDeltaSinceWindowsEpoch().InMicroseconds();
  const int64_t remove_end_us =
      remove_end.ToDeltaSinceWindowsEpoch().InMicroseconds();

  // If |url_filter| is null, remove all records in given date range.
  if (!url_filter) {
    sql::Statement s(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "DELETE FROM compromised_credentials WHERE "
                                "create_time >= ? AND create_time < ?"));
    s.BindInt64(0, remove_begin_us);
    s.BindInt64(1, remove_end_us);
    return s.Run();
  }

  // Otherwise, filter signon_realms.
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT DISTINCT url FROM compromised_credentials WHERE "
      "create_time >= ? AND create_time < ?"));
  s.BindInt64(0, remove_begin_us);
  s.BindInt64(1, remove_end_us);

  std::vector<std::string> signon_realms;
  while (s.Step()) {
    std::string signon_realm = s.ColumnString(0);
    if (url_filter.Run(GURL(signon_realm))) {
      signon_realms.push_back(std::move(signon_realm));
    }
  }

  bool success = true;
  for (const std::string& signon_realm : signon_realms) {
    sql::Statement s(db_->GetCachedStatement(
        SQL_FROM_HERE,
        "DELETE FROM compromised_credentials WHERE url = ? "
        "AND create_time >= ? AND create_time < ?"));
    s.BindString(0, signon_realm);
    s.BindInt64(1, remove_begin_us);
    s.BindInt64(2, remove_end_us);
    success = success && s.Run();
  }
  return success;
}

std::vector<CompromisedCredentials> CompromisedCredentialsTable::GetAllRows() {
  if (!db_)
    return std::vector<CompromisedCredentials>{};

  DCHECK(db_->DoesTableExist(kCompromisedCredentialsTableName));

  static constexpr char query[] = "SELECT * FROM compromised_credentials";
  sql::Statement s(db_->GetCachedStatement(SQL_FROM_HERE, query));
  return StatementToCompromisedCredentials(&s);
}

void CompromisedCredentialsTable::ReportMetrics(BulkCheckDone bulk_check_done) {
  if (!db_)
    return;
  sql::Statement s(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT COUNT(*) FROM compromised_credentials "
                              "WHERE compromise_type = ? "));
  s.BindInt(0, static_cast<int>(CompromiseType::kLeaked));
  if (s.Step()) {
    int count = s.ColumnInt(0);
    base::UmaHistogramCounts100(
        "PasswordManager.CompromisedCredentials.CountLeaked", count);
    if (bulk_check_done) {
      base::UmaHistogramCounts100(
          "PasswordManager.CompromisedCredentials.CountLeakedAfterBulkCheck",
          count);
    }
  }

  s.Reset(true);
  s.BindInt(0, static_cast<int>(CompromiseType::kPhished));
  if (s.Step()) {
    int count = s.ColumnInt(0);
    base::UmaHistogramCounts100(
        "PasswordManager.CompromisedCredentials.CountPhished", count);
  }
}

std::vector<CompromisedCredentials>
CompromisedCredentialsTable::GetRowByCompromiseType(
    const std::string& signon_realm,
    const base::string16& username,
    const CompromiseType& compromise_type) const {
  if (!db_ || signon_realm.empty())
    return std::vector<CompromisedCredentials>{};
  DCHECK(db_->DoesTableExist(kCompromisedCredentialsTableName));

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT url, username, create_time, compromise_type FROM "
      "compromised_credentials WHERE url = ? AND username = ? AND "
      "compromise_type = ?"));
  s.BindString(0, signon_realm);
  s.BindString16(1, username);
  s.BindInt64(2, static_cast<int>(compromise_type));
  return StatementToCompromisedCredentials(&s);
}

}  // namespace password_manager
